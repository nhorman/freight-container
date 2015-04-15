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
#include <errno.h>
#include <string.h>
#include <ftw.h>
#include <manifest.h>
#include <package.h>

static char worktemplate[256];
static char *workdir;
static char tmpdir[512];


static int remove_path(const char *fpath, const struct stat *sb, int typeflag,
			struct FTW *ftwbuf)
{
	if (typeflag == FTW_F)
		return unlink(fpath);

	return rmdir(fpath);
}

static void yum_cleanup()
{
	nftw(workdir, remove_path, 10, FTW_DEPTH);
	return;
}

static int build_path(const char *path)
{
	strcpy(tmpdir, workdir);
	strcat(tmpdir, path);
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

static int run_command(char *cmd)
{
	int rc;
	FILE *yum_out;
	char buf[128];
	size_t count;
	
	yum_out = popen(cmd, "r");
	if (yum_out == NULL) {
		rc = errno;
		fprintf(stderr, "Unable to exec yum for install: %s\n", strerror(rc));
		return rc;
	}

	while(!feof(yum_out) && !ferror(yum_out)) {
		count = fread(buf, 1, 128, yum_out);
		fwrite(buf, count, 1, stderr);
	}

	rc = pclose(yum_out);

	if (rc == -1) {
		rc = errno;
		fprintf(stderr, "yum command failed: %s\n", strerror(rc));
		return rc;
	}

	return 0;
}

static int yum_build_srpm(const struct manifest *manifest)
{
	int rc = -EINVAL;
	char cmd[1024];

	snprintf(cmd, 512, "tar -C %s -jcf %s/%s-freight.tbz2 ./etc/\n",
		workdir, workdir, manifest->package.name);
	rc = run_command(cmd);
	if (rc)
		goto out;

	snprintf(cmd, 512, "rpmbuild -D \"_sourcedir %s\" -D \"_srcrpmdir %s\" "
		"-bs %s/%s-freight-container.spec\n",
		workdir,
		manifest->opts.output_path ? manifest->opts.output_path : workdir,
		workdir, manifest->package.name);

	fprintf(stderr, "%s\n", cmd);
	rc = run_command(cmd);
	if (rc)
		goto out;
out:
	return rc;
}

static int yum_init(const struct manifest *manifest)
{
	struct repository *repo;
	FILE *repof;
	char *rpmlist;
	
	getcwd(worktemplate, 256);
	strcat(worktemplate, "/freight-builder.XXXXXX"); 

	workdir = mkdtemp(worktemplate);
	if (workdir == NULL) {
		fprintf(stderr, "Cannot create temporary work directory %s: %s\n",
			worktemplate, strerror(errno));
		return -EINVAL;
	}

	fprintf(stderr, "Initalizing work directory %s\n", workdir);

	if (build_path("/etc")) {
		fprintf(stderr, "Cannot create etc directory: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	if (build_path("/etc/yum.repos.d")) {
		fprintf(stderr, "Cannot create repository directory: %s\n",
			strerror(errno)); 
		goto cleanup_tmpdir;
	}

	if (build_path("/cache")) {
		fprintf(stderr, "Cannot create cache directory: %s\n",
			strerror(errno)); 
		goto cleanup_tmpdir;
	}

	if (build_path("/logs")) {
		fprintf(stderr, "Cannot create log directory: %s\n",
			strerror(errno)); 
		goto cleanup_tmpdir;
	}

	/*
 	 * for each item in the repos list
 	 * lets create a file with that repository listed
 	 */
	repo = manifest->repos;
	while (repo) {
		strcpy(tmpdir, workdir);
		strcat(tmpdir, "/etc/yum.repos.d/");
		strcat(tmpdir, repo->name);
		strcat(tmpdir, "-fb.repo");
		repof = fopen(tmpdir, "w");
		if (!repof) {
			fprintf(stderr, "Error opening %s: %s\n",
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
	strcpy(tmpdir, workdir);
	strcat(tmpdir, "/etc/yum.conf");
	repof = fopen(tmpdir, "w");
	if (!repof) {
		fprintf(stderr, "Unable to create a repo configuration: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}
	fprintf(repof, "[main]\n");
	fprintf(repof, "cachedir=/cache\n");
	fprintf(repof, "keepcache=1\n");
	fprintf(repof, "logfile=/logs/yum.log\n");
	fprintf(repof, "reposdir=/etc/yum.repos.d/\n");
	fclose(repof);

	/*
 	 * Build the spec file
 	 */
	rpmlist = build_rpm_list(manifest);
	if (!rpmlist)
		goto cleanup_tmpdir;

	sprintf(tmpdir, "%s/%s-freight-container.spec", workdir,
		manifest->package.name);

	repof = fopen(tmpdir, "w");
	if (!repof) {
		fprintf(stderr, "Unable to create a spec file: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	/*
 	 * This builds out our spec file for the source RPM
 	 * We start with the usual tags
 	 */
	fprintf(repof, "Name: %s-freight-container\n",
		manifest->package.name);
	fprintf(repof, "Version: %s\n", manifest->package.version);
	fprintf(repof, "Release: %s\n", manifest->package.release);
	fprintf(repof, "License: %s\n", manifest->package.license);
	fprintf(repof, "Summary: %s\n", manifest->package.summary);
	fprintf(repof, "\n\n");

	/*
 	 * buildrequires include yum as we're going to install a tree with it 
 	 */
	fprintf(repof, "BuildRequires: yum\n");

	fprintf(repof, "\n\n");
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
	fprintf(repof, "yum -y --installroot=${RPM_BUILD_ROOT} install %s\n",
		rpmlist); 
	free(rpmlist);
	fprintf(repof, "yum --installroot=${RPM_BUILD_ROOT} clean all\n");
	/*
 	 * After yum is done installing, we need to interrogate all the files
 	 * So that we can specify a file list in the %files section
 	 */
	fprintf(repof, "for i in `find . -type d`\n");
	fprintf(repof, "do\n"); 
	fprintf(repof, "	echo \"%%dir /$i\" >> %s.manifest\n",
		manifest->package.name);
	fprintf(repof, "done\n");
	fprintf(repof, "for i in `find . -type f`\n");
	fprintf(repof, "do\n"); 
	fprintf(repof, "	echo \"/$i\" >> %s.manifest\n",
		manifest->package.name);
	fprintf(repof, "done\n");
	fprintf(repof, "\n\n");
	fprintf(repof, "%%files -f %s.manifest\n",
		manifest->package.name);

	/*
 	 * And an empty chagnelog
 	 */
	fprintf(repof, "\n\n");
	fprintf(repof, "%%changelog\n");
	fclose(repof);

	return 0;
cleanup_tmpdir:
	yum_cleanup();
	return -EINVAL;
}

struct pkg_ops yum_ops = {
	.init = yum_init,
	.cleanup = yum_cleanup,
	.build_srpm = yum_build_srpm
};


