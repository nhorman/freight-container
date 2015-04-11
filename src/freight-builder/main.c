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

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"manifest", 1, NULL, 'm'},
	{"keep", 1, NULL, 'k'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	fprintf(stderr, "%s [-h | --help] [-k | --keep] <[-m | --manifest]  config>\n", argv[0]);
#else
	frpintf(stderr, "%s [-h] [-k] <-m config>\n", argv[0]);
#endif
}

int main(int argc, char **argv)
{
	int opt, longind;
	struct manifest manifest;
	char *config = NULL;
	int rc = 1;
	struct pkg_ops *build_env;
	int keep = 0;

	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, "h,m:k", lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, "h,m:k") != -1) {
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
		}
	}

	/*
 	 * Do some sanity checks
 	 */
	if (config == NULL) {
		fprintf(stderr, "Need to specify a manifest file\n");
		goto out;
	}

	/*
 	 * Parse the manifest file out
 	 */
	if (read_manifest(config, &manifest)) {
		goto out_release;
	}

	/*
 	 * Setup the builder working env
 	 */
	build_env = init_pkg_mgmt(PKG_YUM, &manifest);
	if (build_env == NULL) {
		fprintf(stderr, "Failed to init build temp directory\n");
		goto out_release;
	}

	/*
 	 * Actually build the image
 	 */
	build_image_from_manifest(build_env, &manifest);	

	/*
 	 * Then cleanup the working space
 	 */
	if (!keep)
		cleanup_pkg_mgmt(build_env);

	rc =0;

out_release:
	release_manifest(&manifest);
out:

	return rc;
}

