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
 *File: dnf.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description: dnf package management implementation
 *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/vfs.h>
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
#define BTRFS_SUPER_MAGIC     0x9123683E

struct rpm_nvr {
	char *name;
	char *version;
	char *release;
	char stringdata[0];
};

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
	__free const char **strings = NULL;

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

	return (strings) ? strvjoin(strings, " ") : NULL;
}

static struct rpm_nvr *get_nvr_from_rpm(const char *rpm)
{
	struct rpm_nvr *nvr = calloc(sizeof(struct rpm_nvr) + strlen(rpm), 1);
	char *bname = basename(rpm);
	char *tmp;

	if (!nvr)
		return NULL;

	nvr->name = nvr->stringdata;
	strncpy(nvr->name, bname, strlen(bname));
	tmp = index(nvr->name, '-');
	*tmp = '\0';
	tmp++;
	nvr->version = tmp;
	tmp = index(nvr->version, '-');
	*tmp = '\0';
	tmp++;
	nvr->release = tmp;
	/* Go back twice from the end to find the full release */
	tmp = rindex(nvr->release, '.');
	*tmp = '\0';
	tmp = rindex(nvr->release, '.');
	*tmp = '\0';

	return nvr;
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

	/*
 	 * This will convert the previously built srpm into a binary rpm that
 	 * can serve as a containerized directory for systemd-nspawn
 	 */
	cmd = strjoina("rpmbuild ", quiet, " -D\"_build_name_fmt ",
			manifest->package.name, "-", manifest->package.version, "-",
			manifest->package.release, ".%%{ARCH}.rpm\" -D\"__arch_install_post "
			"/usr/lib/rpm/check-rpaths /usr/lib/rpm/check-buildroot\" -D\"_rpmdir ",
			output_path, "\" -D \"_topdir ", workdir, "\" --rebuild --noclean --nocheck ",
			output_path, "/", manifest->package.name, "-", manifest->package.version, "-",
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
	struct statfs buf;

	getcwd(path, 256);

	if (manifest->opts.workdir)
		worktemplate = strjoin(manifest->opts.workdir,
				       "/freight-builder.XXXXXX", NULL);
	else
		worktemplate = strjoin(path,
				       "/freight-builder.XXXXXX", NULL);

	if (!worktemplate)
		return -ENOMEM;

	workdir = mkdtemp(worktemplate);
	if (workdir == NULL) {
		LOG(ERROR, "Cannot create temporary work directory %s: %s\n",
			worktemplate, strerror(errno));
		return -EINVAL;
	}

	if (statfs(worktemplate, &buf) == -1) {
		LOG(ERROR, "Cannnot interrogate working directory %s: %s\n",
			worktemplate, strerror(errno));
		return -EINVAL;
	}

	if (buf.f_type != BTRFS_SUPER_MAGIC) {
		LOG(ERROR, "Build directory must be btrfs\n");
		return -EINVAL;
	}

	return 0;
}

static void run_post_script_in_spec(FILE *repof, const struct manifest *manifest)
{

	fprintf(repof, "export FREIGHT_CONTAINERFS=${RPM_BUILD_ROOT}/"
		       "containers/%s/containerfs/\n",
		       manifest->package.name);
	fprintf(repof, "echo executing post script\n");
	fprintf(repof, "chmod 755 %%{SOURCE1}\n");
	fprintf(repof, "script -f -c %%{SOURCE1}\n");


}

static void gather_files_for_spec(FILE *repof, const struct manifest *manifest)
{
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
	fprintf(repof, "cd -\n\n");
}

static int spec_install_primary_container(FILE * repof, const struct manifest *manifest)
{
	char *rpmlist;
	int rc = -EINVAL;


	rpmlist = build_rpm_list(manifest);
	if (!rpmlist)
		goto out;

	fprintf(repof, "%%install\n");
	fprintf(repof, "cd ${RPM_BUILD_ROOT}\n");
	fprintf(repof, "mkdir -p containers/%s\n", manifest->package.name);
	/*
 	 * Create a subvolume to snapshot post install
 	 */
	fprintf(repof, "btrfs subvolume create containers/%%{name}/containerfs\n");

	fprintf(repof, "tar -C ./containers/%s/containerfs/ -x -v -f %%{SOURCE0}\n", manifest->package.name);
	fprintf(repof, "dnf -y --installroot=${RPM_BUILD_ROOT}/containers/%s/containerfs/ "
		       " --nogpgcheck --releasever=%s install %s\n",
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
		fprintf(repof, "dnf -y --installroot=${RPM_BUILD_ROOT}/containers/"
			"%s/containerfs/ --nogpgcheck install shadow-utils\n",
			manifest->package.name); 
		fprintf(repof, "chroot ${RPM_BUILD_ROOT}"
			       "/containers/%s/containerfs "
			       "/usr/sbin/useradd %s\n",
			       manifest->package.name, manifest->copts.user);
	}

	if (manifest->package.post_script)
		run_post_script_in_spec(repof, manifest);

	fprintf(repof, "cp %%{SOURCE2} ${RPM_BUILD_ROOT}/containers/%s/\n",
		manifest->package.name);
	/*
 	 * This needs to hapen last so we can clean out the yum cache
 	 */
	fprintf(repof, "dnf --installroot=${RPM_BUILD_ROOT}/containers/%s/containerfs/ clean all\n",
		manifest->package.name);


	/*
 	 * Now that the install is done, lets take a snapshot of the image
 	 */
	fprintf(repof, "mkdir containers/%s/snapshot\n",
		       manifest->package.name);
	fprintf(repof, "btrfs subvolume snapshot -r containers/%s/containerfs "
		       "containers/%s/snapshot/containerfs\n", manifest->package.name,
		       manifest->package.name);

	/*
 	 * once we have the snapshot, we export it to a file
 	 */
	fprintf(repof, "btrfs send -f containers/%s/btrfs.img "
		       "containers/%s/snapshot/containerfs\n",
		       manifest->package.name, manifest->package.name);

	/*
 	 * Then delete the real subvolumes
 	 */
	fprintf(repof, "btrfs subvolume delete containers/%s/containerfs\n",
		       manifest->package.name);
	fprintf(repof, "btrfs subvolume delete containers/%s/snapshot/containerfs\n",
		       manifest->package.name);
	fprintf(repof, "rm -rf containers/%s/snapshot\n",
		       manifest->package.name);

	/*
 	 * After we export the subvolume, we need to interrogate all the files
 	 * So that we can specify a file list in the %files section
 	 */
	gather_files_for_spec(repof, manifest);

	rc = 0;
out:
	return rc;
}

static int spec_install_derivative_container(FILE * repof, const struct manifest *manifest)
{
	char *rpmlist;
	int rc = -EINVAL;


	/*
 	 * Note that its ok for rpmlist to be NULL here, as installing rpms is
 	 * optional for a derivative container
 	 */
	rpmlist = build_rpm_list(manifest);

	if (!manifest->package.post_script) {
		LOG(ERROR, "Derivative containers must specify a post_script option\n");
		goto out;
	}

	fprintf(repof, "%%install\n");
	fprintf(repof, "cd ${RPM_BUILD_ROOT}\n");


	/*
 	 * Make an area to install our parent container
 	 */
	fprintf(repof, "mkdir -p parent\n");

	/*
 	 * Then extract our parent container
 	 */
	fprintf(repof, "rpm2cpio %s | cpio -i --to-stdout *btrfs.img "
		       " > ${RPM_BUILD_ROOT}/parent/btrfs.img\n",
		       manifest->package.parent_container);

	/*
 	 * Turn the parent back into a subvolume
 	 */
	fprintf(repof, "btrfs receive -f ./parent/btrfs.img "
		       "./parent/\n");

	
	/*
 	 * Then create a snapshot of it to edit
 	 */
	fprintf(repof, "mkdir -p containers/%s/\n", manifest->package.name);

	/*
 	 * Create our derivative snapshot
 	 */
	fprintf(repof, "btrfs subvolume snapshot ./parent/containerfs "
		       "containers/%%{name}/containerfs\n"); 


	/*
 	 * If we added any new repos, they are in the SOURCE0 tarball.  Add them
 	 * now
 	 */
	fprintf(repof, "tar -C containers/%s/containerfs/ -x -v -f  %%{SOURCE0}\n",
		manifest->package.name);

	/*
 	 * If rpmlist is not empty, install those rpms now
 	 */
	if (rpmlist)
		fprintf(repof, "dnf -y --installroot=${RPM_BUILD_ROOT}/"
			       "containers/%s/containerfs/ --releasever=%s "
			       "--nogpgcheck install %s\n",
				manifest->package.name,
				manifest->yum.releasever, rpmlist); 
	free(rpmlist);

	/*
 	 * Finally, we get to run out post script to modify the container
 	 * Note that we have to have a post_script here, and check for it 
 	 * at the start of this function
 	 */
	run_post_script_in_spec(repof, manifest);


	fprintf(repof, "cp %%{SOURCE2} ${RPM_BUILD_ROOT}/containers/%s/\n",
		manifest->package.name);

	/*
 	 * Once we're done, set the new container read only
 	 */
	fprintf(repof, "btrfs property set -ts containers/%%{name}/containerfs "
		       "ro true\n");

	/*
 	 * Now send the read-only snapshot to a file specifying the parent 
 	 * so that we get an incremental image 
 	 */
	fprintf(repof, "btrfs send -p ./parent/containerfs "
		       "-f containers/%%{name}/btrfs.img "
		       "containers/%%{name}/containerfs\n");

	/*
 	 * Now we have to destroy the mounted subvolumes
 	 */
	fprintf(repof, "btrfs subvolume delete ./parent/containerfs\n");
	fprintf(repof, "btrfs subvolume delete ./containers/%%{name}/containerfs\n");
	fprintf(repof, "rm -rf ./parent\n");

	gather_files_for_spec(repof, manifest);
	rc = 0;
out:
	return rc;
}

static int build_spec_file(const struct manifest *manifest)
{
	FILE *repof;
	int rc = -EINVAL;
	char *tmpdir;
	struct rpm_nvr *nvr;

	tmpdir = strjoina(workdir, "/", manifest->package.name, ".spec");
	repof = fopen(tmpdir, "w");
	if (!repof) {
		LOG(ERROR, "Unable to create a spec file: %s\n",
			strerror(errno));
		goto out_noclose;
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
	fprintf(repof, "Source2: container_config\n");

	fprintf(repof, "\n\n");

	if (manifest->package.parent_container) {
		nvr = get_nvr_from_rpm(manifest->package.parent_container);
		if (!nvr) {
			LOG(ERROR, "Cant parse parent_container info\n");
			goto out;
		}
		fprintf(repof, "Requires: %s = %s-%s\n",
			nvr->name, nvr->version, nvr->release);

		free(nvr);
	}

	/*
 	 * buildrequires include yum as we're going to install a tree with it 
 	 */
	fprintf(repof, "BuildRequires: dnf\n");
	fprintf(repof, "BuildRequires: btrfs-progs\n");

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
	if (!manifest->package.parent_container)
		rc = spec_install_primary_container(repof, manifest);
	else
		rc = spec_install_derivative_container(repof, manifest);
	if (rc)
		goto out;

	/*
 	 * Spec %post section
 	 * Note: %post scripts here need to be run with --nochroot specified
 	 * because wherever freight-agent installs them won't have a working
 	 * /bin/sh
 	 */
	fprintf(repof, "%%post\n"
		       "btrfs receive -m / "
		       "-f containers/%%{name}/btrfs.img "
		       "containers/%%{name}/\n"
		       "btrfs property set -ts "
		       "containers/%%{name}/containerfs ro false\n");

	fprintf(repof, "\n\n");

	fprintf(repof, "%%preun\n"
		       "btrfs subvolume delete "
		       "containers/%%{name}/containerfs\n");

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
	rc = 0;
out:
	fclose(repof);
out_noclose:
	return rc;
}

static int stage_workdir(const struct manifest *manifest)
{
	struct repository *repo;
	FILE *repof;
	char pbuf[1024];
	config_t config;
	config_setting_t *tmp;
	char *tmpdir;
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

	sprintf(pbuf, "%s/container_config",
		workdir);

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
		tmpdir = strjoina(workdir, "/containers/", manifest->package.name,
				  "/containerfs/etc/yum.repos.d/", repo->name, "-fb.repo");
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
 	 * Note, we only do this for a parent container.  We only add
 	 * repositories for child containers above
 	 */
	if (!manifest->package.parent_container) {
		tmpdir = strjoina(workdir, "/containers/", manifest->package.name,
				  "/containerfs/etc/yum.conf");
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
	}

	/*
 	 * Build the spec file
 	 */
	if (build_spec_file(manifest))
		goto cleanup_tmpdir;

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
	cmd = strjoina("tar -C ", workdir, "/containers/", manifest->package.name,
		       "/containerfs -jcf ", workdir, "/", manifest->package.name,
		       "-freight.tbz2 .\n");
	
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

	rpmcmd = strjoina("dnf --installroot=", workdir, "/instrospect -y --nogpgcheck ",
			  "--releasever=", mfst->yum.releasever, " install", rpm, "\n", NULL);
	LOG(INFO, "Unpacking container\n");
	rc = run_command(rpmcmd, mfst->opts.verbose);
	if (rc) {
		LOG(ERROR, "Unable to install container rpm\n");
		goto out;
	}

	LOG(INFO, "Container name is %s\n", container_name);
	rpmcmd = strjoina("dnf --installroot ", workdir, "/introspect/containers/", container_name, 
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


