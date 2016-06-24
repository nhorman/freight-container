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
#include <string.h>
#include <config.h>
#include <freight-common.h>
#include <freight-config.h>
#include <freight-db.h>
#include <node.h>
#include <scheduler.h>

struct agent_config config;

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"config", 1, NULL, 'c'},
	{"mode", 1, NULL, 'm'},
	{"noreset", 0, NULL, 'n'},
	{"list", 1, NULL, 'l'},
	{"verbose", 0, NULL, 'v'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	LOG(INFO, "%s [-h | --help] "
		"[-c | --config=<config>] "
		"[-m] | --mode=<mode>] "
		"[-l | --list=all|local|running] \n", argv[0]);
#else
	frpintf(stderr, "%s [-h] [-c <config>] "
			"[-m <mode> ] "
			"[-l all|local|running] \n ", argv[0]);
#endif
}

int main(int argc, char **argv)
{
	int rc = 1;
	int opt, longind;
	char *config_file = "/etc/freight-agent/config";
	char *mode = NULL;
	char *list = "all";
	int verbose = 0;	
	int reset_agent_space = 1;
	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, "hnc:m:l:v", lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, "hnc:m:l:v") != -1) {
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
		case 'm':
			mode = optarg;
			break;
		case 'n':
			reset_agent_space = 0;
			break;
		case 'l':
			list = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		}
	}


	if (strcmp(list, "all") &&
	    strcmp(list, "local") &&
	    strcmp(list, "running")) {
		LOG(ERROR, "list option must be all,running or local\n");
		goto out;
	}

	/*
 	 * Read in the configuration file
 	 */
	rc = read_configuration(config_file, &config);
	if (rc)
		goto out_release;

	config.cmdline.verbose = verbose;
	config.cmdline.reset_agent_space = reset_agent_space;

	/*
 	 * Sanity checks
 	 */
	if (!mode) {
		LOG(ERROR, "You must specify a mode\n");
		goto out_release;
	}

	if (streq(mode, "scheduler"))
		config.cmdline.mode = OP_MODE_SCHEDULER;
	else if (streq(mode, "node"))
		config.cmdline.mode = OP_MODE_NODE;
	else if (streq(mode, "init"))
		config.cmdline.mode = OP_MODE_INIT; 
	else if (streq(mode, "clean"))
		config.cmdline.mode = OP_MODE_CLEAN;
	else {
		LOG(ERROR, "Invalid mode spcified\n");
		goto out_release;
	}

	if (config.node.hostname_override)
		strncpy(config.cmdline.hostname, config.node.hostname_override, 128);
	else if (gethostname(config.cmdline.hostname, 128)) {
		LOG(WARNING, "Unable to get hostname, some operations may not work");
	}

	if (!config.node.container_root) {
		LOG(ERROR, "Mode requires a container_root specification\n");
		goto out_release;
	}

	if (!get_db_api(&config)) {
		LOG(ERROR, "No DB configuration selected\n");
		goto out_release;
	}

	if (db_init(&config)) {
		LOG(ERROR, "Unable to initalize DB subsystem\n");
		goto out_release;
	}

	if (db_connect(&config)) {
		LOG(ERROR, "Unable to connect to databse\n");
		goto out_cleanup_db;
	}
	

	/*
 	 * Enter the appropriate function loop based on mode
 	 * Note, for all modes other than node mode (in which we 
 	 * monitor the database for mutiple tennant requests, all the modes
 	 * here are for the local user only, meaning everything goes to the 
 	 * 'local' tennant
 	 */
	switch (config.cmdline.mode) {

	case OP_MODE_INIT:
		rc = init_container_root(&config);
		if (rc) {
			LOG(ERROR, "Init of container root failed: %s\n",
				strerror(rc));
			goto out_disconnect;
		}
		break;
	case OP_MODE_CLEAN:
		LOG(INFO, "Removing container root %s\n",
			  config.node.container_root);
		clean_container_root(&config);
		break;
	case OP_MODE_NODE:
		rc = enter_mode_loop(&config);
		if (rc) {
			LOG(ERROR, "Node operation terminated abnormally: %s\n",
				strerror(rc));
			goto out_disconnect;
		}
		break;

	case OP_MODE_SCHEDULER:
		if (config.db.dbtype != DB_TYPE_POSTGRES) {
			LOG(ERROR, "Scheduler mode requires a postgres database\n");
			goto out_disconnect;
		}

		rc = enter_scheduler_loop(&config);
		if (rc) {
			LOG(ERROR, "Scheduler operation terminated abnormally: %s\n",
					strerror(rc));
			goto out_disconnect;
		}
		break;
	}

	rc = 0;

out_disconnect:
	db_disconnect(&config);
out_cleanup_db:
	db_cleanup(&config);
out_release:
	release_configuration(&config);
out:
	return rc;
}

