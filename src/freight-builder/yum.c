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

static char *worktemplate;
static char *workdir;

static void yum_cleanup()
{
	recursive_dir_cleanup(workdir);
	return;
}

static int build_path(const char *path)
{
	char *tmpdir = strjoina(workdir, "/", path, NULL);
	return mkdir(tmpdir, 0700);
}

static char *build_rpm_list(const struct manifest *manifest)
{
	unsigned i;
	size_t a = 0;
	struct rpm *rpm;
	__free_strv const char **strings = NULL;

	if (!manifest)
		return NULL;

	rpm = manifest->rpms;
	for (i = 0; rpm; i++) {
		if(!__realloc(strings, a, i+1))
			return NULL;

		strings[i] = rpm->name;
		strings[i+1] = NULL;
		rpm = rpm->next;
	}

	return strvjoin(strings, " ");
}

/*
 * Turn the srpm into an container rpm for freight
 */
static int yum_build_rpm(const struct manifest *manifest)
{
	char *cmd;
	char *output_path;
	struct utsname utsdata;
	int rc;
	char *quiet = manifest->opts.verbose ? "" : "--quiet";
	output_path = manifest->opts.output_path ?: workdir;

	uname(&utsdata);

	/*
 	 * Set QA_RPATHS properly to prevent rpm build warnings about standard
 	 * paths in the RPM.  That has to happen for containers, so the warning
 	 * is expected
 	 */
	setenv("QA_RPATHS", "0x0001", 1);

	cmd = strjoina("rpmbuild ", quiet, " -D\"_build_name_fmt ",
			manifest->package.name, "-", manifest->package.version, "-",
			manifest->package.release, ".%%{ARCH}.rpm\" -D\"__arch_install_post "
			"/usr/lib/rpm/check-rpaths /usr/lib/rpm/check-buildroot\" -D\"_rpmdir ",
			output_path, "\" --rebuild ", output_path, "/",
			manifest->package.name, "-", manifest->package.version, "-",
			manifest->package.release, ".src.rpm\n", NULL);
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
	char path[1024];
	getcwd(path, 1024);
	worktemplate = strjoin(path, "/freight-builder.XXXXXX", NULL);
	if (!worktemplate)
		return -ENOMEM;

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
	char *tmpdir;
	char *pbuf;
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

	pbuf = strjoina("containers/", manifest->package.name, NULL); 
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
	if (manifest->copts.user) {
		tmp = config_setting_add(tmp, "user", CONFIG_TYPE_STRING);
		if (!tmp) {
			LOG(ERROR, "Cannot create user setting in config\n");
			goto cleanup_tmpdir;
		}
		if (config_setting_set_string(tmp, manifest->copts.user) == CONFIG_FALSE) {
			LOG(ERROR, "Can't set user setting value\n");
			goto cleanup_tmpdir;
		}
	}
	pbuf = strjoina(workdir, "/containers/", manifest->package.name, "/container_config", NULL);
	if (config_write_file(&config, pbuf) == CONFIG_FALSE) {
		LOG(ERROR, "Failed to write %s: %s\n",
			pbuf, config_error_text(&config));
		goto cleanup_tmpdir;
	}
	config_destroy(&config);

	while(dirlist[i] != NULL) {
		pbuf = strjoin("containers/", manifest->package.name, "/", dirlist[i], NULL);
		if (!pbuf)
			goto cleanup_nomem;

		if (build_path(pbuf)) {
			LOG(ERROR, "Cannot create %s directory: %s\n",
				dirlist[i], strerror(errno));
			free(pbuf);
			goto cleanup_tmpdir;
		}
		i++;
		free(pbuf);
	}

	/*
 	 * for each item in the repos list
 	 * lets create a file with that repository listed
 	 */
	repo = manifest->repos;
	while (repo) {
		tmpdir = strjoin(workdir, "/containers/", manifest->package.name, "/containerfs/etc/yum.repos.d/",
				 repo->name, "-fb.repo", NULL);
		if (!tmpdir)
			goto cleanup_nomem;

		repof = fopen(tmpdir, "w");
		if (!repof) {
			LOG(ERROR, "Error opening %s: %s\n",
				tmpdir, strerror(errno));
			free(tmpdir);
			goto cleanup_tmpdir;
		}

		fprintf(repof, "[%s-fb]\n", repo->name);
		fprintf(repof, "name=%s-fb\n", repo->name);
		fprintf(repof, "baseurl=%s\n", repo->url);
		fprintf(repof, "gpgcheck=0\n"); /* for now */
		fprintf(repof, "enabled=1\n");
		fclose(repof);
		free(tmpdir);
		repo = repo->next;
	}

	/*
 	 * create a base yum configuration
 	 */
	tmpdir = strjoina(workdir, "/containers/", manifest->package.name,
			  "/containerfs/etc/yum.conf", NULL);
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

	tmpdir = strjoina(workdir, "/", manifest->package.name, ".spec", NULL);
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
	if (manifest->package.post_script)
		fprintf(repof, "Source1: post_script\n");
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

	if (manifest->package.post_script) {
		fprintf(repof, "echo executing post script\n");
		fprintf(repof, "cp %%{SOURCE1} ${RPM_BUILD_ROOT}"
			       "/containers/%s/containerfs/\n",
			       manifest->package.name);
		fprintf(repof, "chmod 755 ${RPM_BUILD_ROOT}"
			       "/containers/%s/containerfs/post_script\n",
			       manifest->package.name);
		fprintf(repof, "chroot ${RPM_BUILD_ROOT}"
			       "/containers/%s/containerfs "
			       "/post_script\n", manifest->package.name);
		fprintf(repof, "rm -f ${RPM_BUILD_ROOT}/containers/%s/"
			       "containerfs/post_script\n",
			       manifest->package.name);
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
cleanup_nomem:
	yum_cleanup();
	return -ENOMEM;

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
	char *cmd;

	LOG(INFO, "SRPM manifest name is %s\n", manifest->package.name);
	rc = stage_workdir(manifest);
	if (rc)
		goto out;

	/*
 	 * Tar up the etc directory we generated in our sandbox.  This gets 
 	 * Included in the srpm as Source0 of the spec file (written in
 	 * yum_init)
 	 */
	cmd = strjoina("tar -C ", workdir, " -jcf ", workdir, "/", 
			manifest->package.name, "-freight.tbz2 ./containers/\n", NULL);
	
	LOG(INFO, "Creating yum configuration tarball for container\n");
	rc = run_command(cmd, manifest->opts.verbose);
	if (rc)
		goto out;

	if (manifest->package.post_script) {
		cmd = strjoina("cp ", manifest->package.post_script, " ", 
				workdir, "/post_script");
		if (run_command(cmd, manifest->opts.verbose))
			goto out;
	}
		
	/*
 	 * Then build the srpms using the spec generated in yum_init.  Point our 
 	 * SOURCES dir at the sandbox to pick up the above tarball, and
 	 * optionally direct the srpm dir to the output directory if it was
 	 * specified
 	 */
	cmd = strjoina("rpmbuild -D \"_sourcedir ", workdir, "\" -D \"_srcrpmdir ",
			manifest->opts.output_path ? manifest->opts.output_path : workdir, "\" "
			"-bs ", workdir, "/", manifest->package.name, ".spec\n", NULL);
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
	char *rpmcmd;
	char *container_name = basename(rpm);

	if (!container_name) {
		LOG(ERROR, "Unable to grab file name from path\n");
		goto out;
	}

	if (build_path("/introspect")) {
		LOG(ERROR, "unable to create introspect directory\n");
		goto out;
	}

	rpmcmd = strjoina("yum --installroot=", workdir, "/instrospect -y --nogpgcheck ",
			  "--releasever=", mfst->yum.releasever, " install", rpm, "\n", NULL);
	LOG(INFO, "Unpacking container\n");
	rc = run_command(rpmcmd, mfst->opts.verbose);
	if (rc) {
		LOG(ERROR, "Unable to install container rpm\n");
		goto out;
	}

	LOG(INFO, "Container name is %s\n", container_name);
	rpmcmd = strjoina("yum --installroot ", workdir, "/introspect/containers/", container_name, 
			  "/containerfs/ --nogpgcheck check-update", NULL);
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


