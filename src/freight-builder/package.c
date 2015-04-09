
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
 *File: package.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015 
 *
 *Description
 * *********************************************************/
#include <stdlib.h>
#include <package.h>


extern struct pkg_ops yum_ops;

struct pkg_ops *init_pkg_mgmt(enum pkg_mgmt_type ptype)
{
	if (ptype >= PKG_MAX)
		return NULL;

	if (yum_ops.init())
		return NULL;

	return &yum_ops;
}

