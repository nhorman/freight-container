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
 *File: freightctl_main.c
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
#include <string.h>
#include <config.h>
#include <freight-log.h>
#include <freight-config.h>
#include <freight-db.h>

struct agent_config config;

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"config", 1, NULL, 'c'},
	{"verbose", 0, NULL, 'v'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	LOG(INFO, "%s [-h | --help] "
		"[-c | --config=<config>] <op>\n", argv[0]);
#else
	frpintf(stderr, "%s [-h] [-c <config>] <op>\n", argv[0];
#endif
}

int main(int argc, char **argv)
{
	int rc = 1;
	int opt, longind;
	char *config_file = "/etc/freight-agent/config";
	char *op;
	struct db_api *api;
	int verbose = 0;

	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, "hc:", lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, "hc:") != -1) {
#endif
		switch(opt) {

		case '?':
			/* FALLTHROUGH */
		case 'h':
			usage(argv);
			goto out;
			/* NOTREACHED */
			break;
		case 'c':
			config_file = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		}
	}

	/*
	 * Read in the configuration file
	 */
	rc = read_configuration(config_file, &config);
	if (rc)
		goto out_release;

	config.cmdline.verbose = verbose;

	/*
 	 * This is the operation to preform
 	 */
	op = argv[optind];
	if (!op) {
		LOG(ERROR, "You must specify an operation\n");
		goto out_release;
	}

	api = get_db_api(&config);
	if (!api) {
		LOG(ERROR, "No DB configuration selected\n");
		goto out_release;
	}

	if (db_init(api, &config)) {
		LOG(ERROR, "Unable to initalize DB subsystem\n");
		goto out_release;
	}

	if (db_connect(api, &config))
		goto out_cleanup_db;
	

	if (!strcmp(op, "repo")) {
		LOG(INFO, "Preforming repo operation\n");	
	} else {
		LOG(ERROR, "Unknown operation\n");
		goto out_disconnect;
	}

	rc = 0;

out_disconnect:
	db_disconnect(api, &config);
out_cleanup_db:
	db_cleanup(api, &config);
out_release:
	release_configuration(&config);
out:
	return rc;
}

