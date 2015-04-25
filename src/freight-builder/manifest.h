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
 *File: manifest.h
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description
 * *********************************************************/

#ifndef _MANIFEST_H_
#define _MANIFEST_H_



/*
 * Child elements for the manifest structure below
 */
struct repository {
	char *name;
	char *url;
	struct repository *next;
};

struct rpm {
	char *name;
	struct rpm *next;
};

struct options {
	char *nspawn_opts;
};

struct packaging {
	char *name;
	char *version;
	char *release;
	char *summary;
	char *license;
	char *author;
};

struct yum_opts {
	char *releasever;
};

struct container_opts {
	char *user;
};

/*
 * Command line options to add after parsing the manifest
 */
struct cmdline_options {
	char *output_path;
	int verbose;
};

/*
 * This represents the set of parsed information we get out of the above
 * manifest files
 */
struct manifest {
	struct repository *repos;
	struct rpm *rpms;
	struct options *options;
	struct packaging package;
	struct yum_opts yum;
	struct cmdline_options opts;
	struct container_opts copts;
};

void release_manifest(struct manifest *manifest);

int read_manifest(char *config_path, struct manifest *manifest);



#endif
