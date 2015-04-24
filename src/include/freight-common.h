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
 *File: freight-common.h 
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description: common utility routines for freight tools 
 *********************************************************/

#ifndef _FREIGHT_COMMON_H_
#define _FREIGHT_COMMON_H_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>

static inline int __remove_path(const char *fpath, const struct stat *sb, int typeflag,
				struct FTW *ftwbuf)
{
	int rc;
	char *reason;
	struct stat buf;

	switch (typeflag) {
	case FTW_F:
	case FTW_SL:
	case FTW_SLN:
	case FTW_NS:
		rc = unlink(fpath);
		break;
	case FTW_DP:
	case FTW_D:
		lstat(fpath, &buf);
		if (buf.st_mode & S_IFLNK)
			rc = unlink(fpath);
		else
			rc = rmdir(fpath);
		break;
	default:
		rc = -EINVAL;
	}

	if (rc) {
		reason = strerror(errno);
		fprintf(stderr, "Could not remove %s: %s", fpath, reason);
	}

	return rc;
	
}

static void recursive_dir_cleanup(const char *path)
{
	nftw(path, __remove_path, 10, FTW_DEPTH|FTW_PHYS);
	return;
}
#endif
