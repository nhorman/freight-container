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

static int yum_init(const struct manifest *manifest)
{
	struct repository *repo;
	FILE *repof;
	
	getcwd(worktemplate, 256);
	strcat(worktemplate, "/freight-builder.XXXXXX"); 

	workdir = mkdtemp(worktemplate);
	if (workdir == NULL) {
		fprintf(stderr, "Cannot create temporary work directory %s: %s\n",
			worktemplate, strerror(errno));
		return -EINVAL;
	}

	fprintf(stderr, "Initalizing work directory %s\n", workdir);

	if (build_path("/buildroot")) {
		fprintf(stderr, "Cannot create buildroot directory: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	if (build_path("/buildroot/etc")) {
		fprintf(stderr, "Cannot create buildroot/etc directory: %s\n",
			strerror(errno));
		goto cleanup_tmpdir;
	}

	if (build_path("/buildroot/etc/yum.repos.d")) {
		fprintf(stderr, "Cannot create repository directory: %s\n",
			strerror(errno)); 
		goto cleanup_tmpdir;
	}

	if (build_path("/buildroot/cache")) {
		fprintf(stderr, "Cannot create cache directory: %s\n",
			strerror(errno)); 
		goto cleanup_tmpdir;
	}

	if (build_path("/buildroot/logs")) {
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
		strcat(tmpdir, "/buildroot/etc/yum.repos.d/");
		strcat(tmpdir, repo->name);
		strcat(tmpdir, "-fb.repo");
		repof = fopen(tmpdir, "w");
		if (!repof) {
			fprintf(stderr, "Error opening %s: %s\n",
				tmpdir, strerror(errno));
			goto cleanup_tmpdir;
		}

		fprintf(repof, "[%s]\n", repo->name);
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
	strcat(tmpdir, "/buildroot/etc/yum.conf");
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

	return 0;
cleanup_tmpdir:
	yum_cleanup();
	return -EINVAL;
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

static char *gen_install_cmd(char *rpmlist)
{
	size_t alloc_size;
	char *result;


	alloc_size = 4; /* yum */
	alloc_size += 3; /* -y */
	alloc_size += 15; /* "/yum.conf" */
	alloc_size += strlen(workdir) + 25; /* --installroot=<workdir>/buildroot */ 
	alloc_size += 10; /* install */
	alloc_size += strlen(rpmlist);

	result = calloc(1, alloc_size);
	if (!result)
		return NULL;

	sprintf(result, "yum -y --installroot=%s/buildroot install %s\n",
		workdir, rpmlist);

	return result;
}

static char *gen_cleanup_cmd()
{
	char *result;
	size_t alloc_size;
	

	alloc_size = 64 + strlen(workdir);

	result = calloc(1, alloc_size);
	if (!result)
		return NULL;

	sprintf(result, "yum -y --installroot=%s/buildroot clean all\n", workdir);

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

static int yum_build(const struct manifest *manifest)
{
	int rc = -EINVAL;
	char *rpmlist;
	char *yum_install, *yum_clean;

	/*
 	 * Pretty easy here, just take all the rpms, get concatinate them
 	 * and pass them to the appropriate yum command
 	 */
	rpmlist = build_rpm_list(manifest);
	if (!rpmlist)
		goto out;

	yum_install = gen_install_cmd(rpmlist);
	if (!yum_install)
		goto out_free_rpmlist;


	fprintf(stderr, "Installing manifest to %s\n", workdir);
	/*
 	 * Lets run yum in the appropriate subdirectory with the right config to
 	 * populate the buildroot directory
 	 */
	if (run_command(yum_install))
		goto out_free_cmd;

	yum_clean = gen_cleanup_cmd();
	if (!yum_clean)
		goto out_free_cmd;

	fprintf(stderr, "Cleaning up temporary files\n");

	if (run_command(yum_clean))
		goto out_free_clean;

	rc = 0;

out_free_clean:
	free(yum_clean);
out_free_cmd:
	free(yum_install);
out_free_rpmlist:	
	free(rpmlist);
out:
	return rc;
}


struct pkg_ops yum_ops = {
	yum_init,
	yum_cleanup,
	yum_build
};


