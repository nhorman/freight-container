/*********************************************************
 *Copyright (C) 2004 Neil Horman
 *This program is free software; you can redistribute it and\or modify
 *it under the terms of the GNU General Public License as published 
 *by the Free Software Foundation; either version 2 of the License,
 *or  any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *GNU General Public License for more details.
 *
 *File: yum.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description: yum package management implementation
 *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <errno.h>
#include <string.h>
#include <libconfig.h>
#include <manifest.h>
#include <package.h>
#include <freight-log.h>
#include <freight-common.h>

static char worktemplate[256];
static char *workdir;
static char tmpdir[1024];

static void yum_cleanup()
{
	recursive_dir_cleanup(workdir);
	return;
}

static int build_path(const char *path)
{
	sprintf(tmpdir, "%s/%s",
		workdir, path);
	return mkdir(tmpdir, 0700);
}

static char *build_rpm_list(const struct manifest *manifest)
{
	size_t alloc_size = 0;
	int count;
	char *result;
	struct rpm *rpm = manifest->rpms;

	while (rpm) {
		/* Add an extra character for the space */
		alloc_size += strlen(rpm->name) + 2;
		rpm = rpm->next;
	}


	result =  calloc(1, alloc_size);
	if (!result)
		return NULL;

	rpm = manifest->rpms;
	count = 0;
	while (rpm) {
		/* Add 2 to include the trailing space */
		count += snprintf(&result[count], strlen(rpm->name)+2 , "%s ", rpm->name);
		rpm = rpm->next;
	}

	return result;
}

/*
 * Turn the srpm into an container rpm for freight
 */
static int yum_build_rpm(const struct manifest *manifest)
{
	char cmd[1024];
	char *output_path;
	struct utsname utsdata;
	int rc;
	char *quiet = manifest->opts.verbose ? "" : "--quiet";

	output_path = manifest->opts.output_path ? manifest->opts.output_path :
			workdir;

	uname(&utsdata);

	/*
 	 * Set QA_RPATHS properly to prevent rpm build warnings about standard
 	 * paths in the RPM.  That has to happen for containers, so the warning
 	 * is expected
 	 */
	setenv("QA_RPATHS", "0x0001", 1);

	/*
 	 * This will convert the previously built srpm into a binary rpm that
 	 * can serve as a containerized directory for systemd-nspawn
 	 */
	snprintf(cmd, 1024, "rpmbuild %s "
		 "-D\"_build_name_fmt "
		 "%s-%s-%s.%%%%{ARCH}.rpm\" "
		 "-D\"__arch_install_post "
		 "/usr/lib/rpm/check-rpaths /usr/lib/rpm/check-buildroot\" "
		 "-D\"_rpmdir %s\" "
		 "--rebuild %s/%s-%s-%s.src.rpm\n",
		 quiet, 
		 manifest->package.name, manifest->package.version,
		 manifest->package.release,
		 output_path, output_path,
		 manifest->package.name, manifest->package.version,
		 manifest->package.release);
	LOG(INFO, "Building container binary rpm\n");
	rc = run_command(cmd, manifest->opts.verbose);
	if (!rc)
		LOG(INFO, "Wrote %s/%s-%s-%s.%s.rpm\n",
			output_path, manifest->package.name,
			manifest->package.version,
			manifest->package.release,
			utsdata.machine);
	return rc;
}

static int yum_init(const struct manifest *manifest)
{
	getcwd(worktemplate, 256);
	strcat(worktemplate, "/freight-builder.XXXXXX"); 

	workdir = mkdtemp(worktemplate);
	if (workdir == NULL) {
		LOG(ERROR, "Cannot create temporary work directory %s: %s\n",
			worktemplate, strerror(errno));
		return -EINVAL;
	}
	return 0;
}

static int stage_workdir(const struct manifest *manifest)
{
	struct repository *repo;
	FILE *repof;
	char *rpmlist;
	char pbuf[1024];
	config_t config;
	config_setting_t *tmp;
	char *dirlist[] = {
		"containerfs",
		"containerfs/etc",
		"containerfs/etc/yum.repos.d",
		"containerfs/var",
		"containerfs/var/cache",
		"containerfs/var/cache/yum",
		"containerfs/var/log",
		NULL,
	};

	int i = 0;

	LOG(INFO, "Initalizing work directory %s\n", workdir);

	if (build_path("containers")) {
		LOG(ERROR, "Cannot create containers directory: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	sprintf(pbuf, "containers/%s", manifest->package.name); 
	if (build_path(pbuf)) {
		LOG(ERROR, "Cannot create container name directory: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	config_init(&config);
	tmp = config_root_setting(&config);
	if (!tmp) {
		LOG(ERROR, "Cannot get root setting for container config\n");
		goto cleanup_tmpdir;
	}
	tmp = config_setting_add(tmp, "container_opts", CONFIG_TYPE_GROUP);
	if (!tmp) {
		LOG(ERROR, "Cannot create container_opts group\n");
		goto cleanup_tmpdir;
	}
	tmp = config_setting_add(tmp, "user", CONFIG_TYPE_STRING);
	if (!tmp) {
		LOG(ERROR, "Cannot create user setting in config\n");
		goto cleanup_tmpdir;
	}
	if (config_setting_set_string(tmp, manifest->copts.user) == CONFIG_FALSE) {
		LOG(ERROR, "Can't set user setting value\n");
		goto cleanup_tmpdir;
	}
	sprintf(pbuf, "%s/containers/%s/container_config",
		workdir, manifest->package.name);
	if (config_write_file(&config, pbuf) == CONFIG_FALSE) {
		LOG(ERROR, "Failed to write %s: %s\n",
			pbuf, config_error_text(&config));
		goto cleanup_tmpdir;
	}
	config_destroy(&config);

	while(dirlist[i] != NULL) {
		sprintf(pbuf, "containers/%s/%s",
			manifest->package.name, dirlist[i]);
		if (build_path(pbuf)) {
			LOG(ERROR, "Cannot create %s directory: %s\n",
				dirlist[i], strerror(errno));
			goto cleanup_tmpdir;
		}
		i++;
	}

	/*
 	 * for each item in the repos list
 	 * lets create a file with that repository listed
 	 */
	repo = manifest->repos;
	while (repo) {
		sprintf(tmpdir, "%s/containers/%s/containerfs/etc/yum.repos.d/%s-fb.repo",
			workdir, manifest->package.name, repo->name);
		repof = fopen(tmpdir, "w");
		if (!repof) {
			LOG(ERROR, "Error opening %s: %s\n",
				tmpdir, strerror(errno));
			goto cleanup_tmpdir;
		}

		fprintf(repof, "[%s-fb]\n", repo->name);
		fprintf(repof, "name=%s-fb\n", repo->name);
		fprintf(repof, "baseurl=%s\n", repo->url);
		fprintf(repof, "gpgcheck=0\n"); /* for now */
		fprintf(repof, "enabled=1\n");
		fclose(repof);
		repo = repo->next;
	}

	/*
 	 * create a base yum configuration
 	 */
	sprintf(tmpdir, "%s/containers/%s/containerfs/etc/yum.conf",
		workdir,manifest->package.name);
	repof = fopen(tmpdir, "w");
	if (!repof) {
		LOG(ERROR, "Unable to create a repo configuration: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}
	fprintf(repof, "[main]\n");
	fprintf(repof, "cachedir=/var/cache/yum\n");
	fprintf(repof, "keepcache=1\n");
	fprintf(repof, "logfile=/var/log/yum.log\n");
	fprintf(repof, "reposdir=/etc/yum.repos.d/\n");
	fclose(repof);

	/*
 	 * Build the spec file
 	 */
	rpmlist = build_rpm_list(manifest);
	if (!rpmlist)
		goto cleanup_tmpdir;

	sprintf(tmpdir, "%s/%s.spec", workdir,
		manifest->package.name);

	repof = fopen(tmpdir, "w");
	if (!repof) {
		LOG(ERROR, "Unable to create a spec file: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	/*
 	 * This builds out our spec file for the source RPM
 	 * We start with the usual tags
 	 */
	fprintf(repof, "Name: %s\n",
		manifest->package.name);
	fprintf(repof, "Version: %s\n", manifest->package.version);
	fprintf(repof, "Release: %s\n", manifest->package.release);
	fprintf(repof, "License: %s\n", manifest->package.license);
	fprintf(repof, "Group: Containers/Freight\n");
	/*
 	 * We don't want these rpms to provide anything that the host system
 	 * might want
 	 */
	fprintf(repof, "AutoReqProv: no\n");
	fprintf(repof, "Summary: %s\n", manifest->package.summary);
	fprintf(repof, "Source0: %s-freight.tbz2\n", manifest->package.name);
	fprintf(repof, "\n\n");

	/*
 	 * buildrequires include yum as we're going to install a tree with it 
 	 */
	fprintf(repof, "BuildRequires: yum\n");
	fprintf(repof, "\n\n");

	/*
 	 * Description section
 	 */
	fprintf(repof, "%%description\n");
	fprintf(repof, "A container rpm for freight\n");
	fprintf(repof, "\n\n");


	/*
 	 * The install section actually has yum do the install to
 	 * the srpms buildroot, that way we can package the containerized
 	 * fs into its own rpm
 	 */
	fprintf(repof, "%%install\n");
	fprintf(repof, "cd ${RPM_BUILD_ROOT}\n");
	fprintf(repof, "tar xvf %%{SOURCE0}\n");
	fprintf(repof, "yum -y --installroot=${RPM_BUILD_ROOT}/containers/%s/containerfs/ "
		       " --releasever=%s install %s\n",
		manifest->package.name, manifest->yum.releasever, rpmlist); 
	free(rpmlist);

	/*
 	 * After we install, we may need to add a user to the
 	 * container
 	 */
	if (manifest->copts.user) {
		/*
 		 * Need to ensure that we have shadowutils installed
 		 */
		fprintf(repof, "yum -y --installroot=${RPM_BUILD_ROOT}/containers/"
			"%s/containerfs/ --nogpgcheck install shadow-utils\n",
			manifest->package.name); 
		fprintf(repof, "chroot ${RPM_BUILD_ROOT}"
			       "/containers/%s/containerfs "
			       "/usr/sbin/useradd %s\n",
			       manifest->package.name, manifest->copts.user);
	}

	/*
 	 * This needs to hapen last so we can clean out the yum cache
 	 */
	fprintf(repof, "yum --installroot=${RPM_BUILD_ROOT}/containers/%s/containerfs/ clean all\n",
		manifest->package.name);
	/*
 	 * After yum is done installing, we need to interrogate all the files
 	 * So that we can specify a file list in the %files section
 	 */
	fprintf(repof, "cd containers\n");
	fprintf(repof, "rm -f /tmp/%s.manifest\n", manifest->package.name);
	fprintf(repof, "for i in `find . -type d | grep -v ^.$`\n");
	fprintf(repof, "do\n"); 
	fprintf(repof, "	echo \"%%dir /containers/$i\" >> /tmp/%s.manifest\n",
		manifest->package.name);
	fprintf(repof, "done\n");

	fprintf(repof, "for i in `find . -type f`\n");
	fprintf(repof, "do\n"); 
	fprintf(repof, "	echo \"/containers/$i\" >> /tmp/%s.manifest\n",
		manifest->package.name);
	fprintf(repof, "done\n");

	fprintf(repof, "for i in `find . -type l`\n");
	fprintf(repof, "do\n"); 
	fprintf(repof, "	echo \"/containers/$i\" >> /tmp/%s.manifest\n",
		manifest->package.name);
	fprintf(repof, "done\n");
	fprintf(repof, "cd -\n");

	/*
 	 * Spec %files section
 	 */
	fprintf(repof, "\n\n");
	fprintf(repof, "%%files -f /tmp/%s.manifest\n",
		manifest->package.name);

	/*
 	 * changelog section
 	 */
	fprintf(repof, "\n\n");
	fprintf(repof, "%%changelog\n");
	fclose(repof);

	return 0;
cleanup_tmpdir:
	yum_cleanup();
	return -EINVAL;
}

/*
 * Build an srpm from our yum config setup we generated
 * in yum_init.
 */
static int yum_build_srpm(const struct manifest *manifest)
{
	int rc = -EINVAL;
	char cmd[1024];

	LOG(INFO, "SRPM manifest name is %s\n", manifest->package.name);
	rc = stage_workdir(manifest);
	if (rc)
		goto out;

	/*
 	 * Tar up the etc directory we generated in our sandbox.  This gets 
 	 * Included in the srpm as Source0 of the spec file (written in
 	 * yum_init)
 	 */
	snprintf(cmd, 1024, "tar -C %s -jcf %s/%s-freight.tbz2 ./containers/\n",
		workdir, workdir, manifest->package.name);
	LOG(INFO, "Creating yum configuration for container\n");
	rc = run_command(cmd, manifest->opts.verbose);
	if (rc)
		goto out;

	/*
 	 * Then build the srpms using the spec generated in yum_init.  Point our 
 	 * SOURCES dir at the sandbox to pick up the above tarball, and
 	 * optionally direct the srpm dir to the output directory if it was
 	 * specified
 	 */
	snprintf(cmd, 512, "rpmbuild -D \"_sourcedir %s\" -D \"_srcrpmdir %s\" "
		"-bs %s/%s.spec\n",
		workdir,
		manifest->opts.output_path ? manifest->opts.output_path : workdir,
		workdir, manifest->package.name);
	LOG(INFO, "Building container source rpm\n");
	rc = run_command(cmd, manifest->opts.verbose);
	if (rc)
		goto out;
	LOG(INFO, "Wrote srpm %s/%s-%s-%s.src.rpm\n",
		manifest->opts.output_path ? manifest->opts.output_path : workdir,
		manifest->package.name, manifest->package.version,
		manifest->package.release);
out:
	return rc;
}

int yum_inspect(const struct manifest *mfst, const char *rpm)
{
	int rc = -EINVAL;
	char rpmcmd[1024];
	char *container_name = basename(rpm);
	char *tmp = strstr(container_name, "");

	if (!container_name) {
		LOG(ERROR, "Unable to grab file name from path\n");
		goto out;
	}

	if (build_path("/introspect")) {
		LOG(ERROR, "unable to create introspect directory\n");
		goto out;
	}
	sprintf(rpmcmd, "yum --installroot=%s/introspect -y --nogpgcheck "
		"--releasever=%s install %s\n",
		workdir, mfst->yum.releasever, rpm);

	LOG(INFO, "Unpacking container\n");
	rc = run_command(rpmcmd, mfst->opts.verbose);
	if (rc) {
		LOG(ERROR, "Unable to install container rpm\n");
		goto out;
	}
	*tmp = '\0'; /* Null Terminate the container name */
	LOG(INFO, "Container name is %s\n", container_name);
	sprintf(rpmcmd, "yum --installroot %s/introspect/containers/%s/containerfs/ --nogpgcheck check-update",
		 workdir, container_name);
	LOG(INFO, "Looking for packages Requiring update:\n");
	rc = run_command(rpmcmd, 1);

	if (rc == 0)
		LOG(ERROR, "All packages up to date\n");
	/*
 	 * yum check-update exist with code 100 if there are updated packages
 	 * which is a success exit code to us
 	 */
	if (rc > 1)
		rc = 0;
out:
	return rc;
}

struct pkg_ops yum_ops = {
	.init = yum_init,
	.cleanup = yum_cleanup,
	.build_srpm = yum_build_srpm,
	.build_rpm = yum_build_rpm,
	.introspect_container = yum_inspect
};


