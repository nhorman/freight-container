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
	int (*init)(const struct manifest *manifest);
	void (*cleanup)();
	int (*build_srpm)(const struct manifest *manifest);
	int (*build_rpm)(const struct manifest *manifest);
	int (*introspect_container)(const struct manifest *mfst,
				    const char *crpm);
};

struct pkg_ops *init_pkg_mgmt(enum pkg_mgmt_type ptype,
				struct manifest *manifest);

static inline void cleanup_pkg_mgmt(struct pkg_ops *ops)
{
	ops->cleanup();
}

static inline int build_srpm_from_manifest(struct pkg_ops *ops,
					   const struct manifest *mfst)
{
	return ops->build_srpm(mfst);
}

static inline int build_rpm_from_srpm(struct pkg_ops *ops,
				      const struct manifest *mfst)
{
	return ops->build_rpm(mfst);
}

static inline int introspect_container_rpm(struct pkg_ops *ops,
					const struct manifest *mfst,
					const char *crpm)
{
	return ops->introspect_container(mfst, crpm);
}

#endif
