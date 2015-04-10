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

static char worktemplate[] = "./freight-builder.XXXXXX";
static char *workdir;
static char tmpdir[256];


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

static int yum_init(struct manifest *manifest)
{
	struct repository *repo;
	FILE *repof;

	workdir = mkdtemp(worktemplate);
	if (workdir == NULL) {
		fprintf(stderr, "Cannot create temporary work directory %s: %s\n",
			worktemplate, strerror(errno));
		return -EINVAL;
	}

	strcpy(tmpdir, workdir);
	strcat(tmpdir, "/yum.repos.d/");
	if (mkdir(tmpdir, 0700)) {
		fprintf(stderr, "Cannot create repository directory %s: %s\n",
			tmpdir, strerror(errno)); 
		goto cleanup_tmpdir;
	}

	/*
 	 * for each item in the repos list
 	 * lets create a file with that repository listed
 	 */
	repo = manifest->repos;
	while (repo) {
		strcpy(tmpdir, workdir);
		strcat(tmpdir, "/yum.repos.d/");
		strcat(tmpdir, repo->name);
		strcat(tmpdir, ".repo");
		repof = fopen(tmpdir, "w");
		if (!repof) {
			fprintf(stderr, "Error opening %s: %s\n",
				tmpdir, strerror(errno));
			goto cleanup_tmpdir;
		}

		fprintf(repof, "[%s]\n", repo->name);
		fprintf(repof, "name=%s\n", repo->name);
		fprintf(repof, "baseurl=%s\n", repo->url);
		fprintf(repof, "gpgcheck=0\n"); /* for now */
		fprintf(repof, "enabled=1\n");
		fclose(repof);
		repo = repo->next;
	}
		
	return 0;
cleanup_tmpdir:
	yum_cleanup();
	return -EINVAL;
}

struct pkg_ops yum_ops = {
	yum_init,
	yum_cleanup
};

