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
 *File: main.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description
 *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <manifest.h>
#include <package.h>
#include <config.h>
#include <freight-log.h>

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"manifest", 1, NULL, 'm'},
	{"keep", 0, NULL, 'k'},
	{"output", 1, NULL, 'o'},
	{"source", 0, NULL, 's'},
	{"check", 1, NULL, 'c'},
	{"verbose", 0, NULL, 'v'},
	{"workdir", 1, NULL, 'w'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	LOG(INFO, "%s [-h | --help] [-o | --output path ] "
			"[-s || --source] "
			"[-k | --keep] "
			"<[-m | --manifest]  config> "
			"<-c | --check=<container rpm>> "
			"[-w | --workdir <dir>] "
			"[-v] | [--verbose]\n", argv[0]);
#else
	LOG(INFO,  "%s [-h] [-k] [-s] [-v] "
			"[-o path] <-m config> <-c container>\n", argv[0]);
#endif
}

#define OPTSTRING "h,m:ko:sc:vw:"

int main(int argc, char **argv)
{
	int opt, longind;
	struct manifest manifest;
	char *config = NULL;
	int rc = 1;
	struct pkg_ops *build_env;
	int keep = 0;
	char *output = NULL;
	int source_only=0;
	char *container_rpm = NULL;
	int verbose = 0;
	char *workdir = NULL;

	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, OPTSTRING, lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, OPTSTRING) != -1) {
#endif
		switch(opt) {

		case '?':
			/* FALLTHROUGH */
		case 'h':
			usage(argv);
			goto out;
			/* NOTREACHED */
			break;
		case 'k':
			keep = 1;
			break;
		case 'm':
			config = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 's':
			source_only = 1;
			break;
		case 'c':
			container_rpm = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			workdir = optarg;
			break;
		}
	}

	/*
 	 * Do some sanity checks
 	 */
	if ((!config) && (!container_rpm)) {
		LOG(ERROR, "Need to specify a manifest or container file\n");
		goto out;
	}

	/*
 	 * Parse the manifest file out
 	 */
	if (read_manifest(config, &manifest)) {
		goto out_release;
	}

	/*
 	 * Add any relevant command line info to the manifest
 	 */
	manifest.opts.output_path = output;
	manifest.opts.verbose = verbose;
	manifest.opts.workdir = workdir;

	/*
 	 * Setup the builder working env
 	 */
	build_env = init_pkg_mgmt(PKG_YUM, &manifest);
	if (build_env == NULL) {
		LOG(ERROR, "Failed to init build temp directory\n");
		goto out_cleanup;
	}

	if (container_rpm) {
		rc = introspect_container_rpm(build_env, &manifest, container_rpm);
		if (rc)
			goto out_cleanup;
	} else {
		/*
		 * Actually build the image
		 */
		build_srpm_from_manifest(build_env, &manifest);	

		/*
		 * If we need to, build the actual container rpm as well
		 */
		if (!source_only)
			build_rpm_from_srpm(build_env, &manifest);
	}

	rc =0;

out_cleanup:
	/*
 	 * Then cleanup the working space
 	 */
	if (!keep)
		cleanup_pkg_mgmt(build_env);
out_release:
	release_manifest(&manifest);
out:

	return rc;
}

