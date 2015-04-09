#ifndef _PACKAGE_H_
#define _PACKAGE_H_

/*********************************************************
 *Copyright (C) 2015 Neil Horman
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
 *File: package.h
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description: defines the interface through which we manage
 * rpm packages and repositories
 *********************************************************/
#include <manifest.h>


/*
 * Typedefs for package management
 */
enum pkg_mgmt_type {
	PKG_YUM = 0,
	PKG_MAX,
};


struct pkg_ops {
	int (*init)(struct manifest *manifest);
	void (*cleanup)();
};

struct pkg_ops *init_pkg_mgmt(enum pkg_mgmt_type ptype,
				struct manifest *manifest);

static inline void cleanup_pkg_mgmt(struct pkg_ops *ops)
{
	ops->cleanup();
}

#endif
