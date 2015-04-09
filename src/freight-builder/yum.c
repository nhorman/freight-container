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
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <manifest.h>
#include <package.h>

static char worktemplate[] = "~/freight-builder.XXXXXX";
static char *workdir;
static char tmpdir[256];

static int yum_init(struct manifest *manifest)
{
	workdir = mkdtemp(worktemplate);
	if (workdir == NULL) {
		return -EINVAL;
	}

	strcpy(tmpdir, workdir);
	strcat(tmpdir, "/yum.repos.d/");
	if (mkdir(tmpdir, 0700)) {
		goto cleanup_tmpdir;
	}
	
	return 0;
cleanup_tmpdir:
	rmdir(workdir);
	return -EINVAL;
}

static void yum_cleanup()
{
	return;
}

struct pkg_ops yum_ops = {
	yum_init,
	yum_cleanup
};

