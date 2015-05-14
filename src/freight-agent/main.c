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
#include <freight-log.h>
#include <freight-config.h>
#include <freight-db.h>
#include <mode.h>

struct agent_config config;

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"config", 1, NULL, 'c'},
	{"mode", 1, NULL, 'm'},
	{"rpm", 1, NULL, 'r'},
	{"list", 1, NULL, 'l'},
	{"verbose", 0, NULL, 'v'},
	{"name", 1, NULL, 'n'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	LOG(INFO, "%s [-h | --help] "
		"[-c | --config=<config>] "
		"[-m] | --mode=<mode>] "
		"[-r | --rpm=<rpm>] "
		"[-n | --name=<name>] "
		"[-l | --list=all|local|running] \n", argv[0]);
#else
	frpintf(stderr, "%s [-h] [-c <config>] "
			"[-m <mode> ] [r <rpm>] "
			"[-n | --name <name>] [-l all|local|running] \n ", argv[0]);
#endif
}

int main(int argc, char **argv)
{
	int rc = 1;
	int opt, longind;
	char *config_file = "/etc/freight-agent/config";
	char *mode = NULL;
	char *rpm = NULL;
	struct db_api *api;
	char *list = "all";
	char *name = NULL;
	int verbose = 0;	
	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, "hc:m:r:l:vn:", lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, "hc:m:r:l:vn:") != -1) {
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
			name = optarg;
			break;
		case 'r':
			rpm = optarg;
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

	/*
 	 * Sanity checks
 	 */
	if (!mode) {
		LOG(ERROR, "You must specify a mode\n");
		goto out_release;
	}

	if (!strcmp(mode, "node"))
		config.cmdline.mode = OP_MODE_NODE;
	else if (!strcmp(mode, "init"))
		config.cmdline.mode = OP_MODE_INIT; 
	else if (!strcmp(mode, "clean"))
		config.cmdline.mode = OP_MODE_CLEAN;
	else if (!strcmp(mode, "install"))
		config.cmdline.mode = OP_MODE_INSTALL;
	else if (!strcmp(mode, "uninstall"))
		config.cmdline.mode = OP_MODE_UNINSTALL;
	else if (!strcmp(mode, "list"))
		config.cmdline.mode = OP_MODE_LIST;
	else if (!strcmp(mode, "exec"))
		config.cmdline.mode = OP_MODE_EXEC;
	else {
		LOG(ERROR, "Invalid mode spcified\n");
		goto out_release;
	}

	if (!config.node.container_root) {
		LOG(ERROR, "Mode requires a container_root specification\n");
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
	

	/*
 	 * Enter the appropriate function loop based on mode
 	 * Note, for all modes other than node mode (in which we 
 	 * monitor the database for mutiple tennant requests, all the modes
 	 * here are for the local user only, meaning everything goes to the 
 	 * 'local' tennant
 	 */
	switch (config.cmdline.mode) {

	case OP_MODE_INIT:
		rc = init_container_root(api, &config);
		if (rc) {
			LOG(ERROR, "Init of container root failed: %s\n",
				strerror(rc));
			goto out_disconnect;
		}
		break;
	case OP_MODE_CLEAN:
		LOG(INFO, "Removing container root %s\n",
			  config.node.container_root);
		clean_container_root(config.node.container_root);
		break;
	case OP_MODE_INSTALL:
		if (!rpm) {
			LOG(ERROR, "Must specify a container name or rpm with "
				   "-r option in install mode\n");
			goto out_disconnect;
		}
		rc = local_install_container(rpm, &config);
		if (rc) {
			LOG(ERROR, "Failed to install container: %s\n",
				strerror(rc));
			goto out_disconnect;
		}
		break;
	case OP_MODE_UNINSTALL:
		if (!rpm) {
			LOG(ERROR, "Must specify the container name with -r\n");
			goto out_disconnect;
		}
		rc = local_uninstall_container(rpm, &config);
		if (rc) {
			LOG(ERROR, "Uninstall of container %s failed: %s\n",
				rpm, strerror(rc));
			goto out_disconnect;
		}
		break;
	case OP_MODE_LIST:
		local_list_containers(list, &config);
		break;
	case OP_MODE_EXEC:
		if (!rpm) {
			LOG(ERROR, "Must specify the container name with -r\n");
			goto out_disconnect;
		}
		if (!name) {
			LOG(ERROR, "Must specify a container instance name with -n\n");
			goto out_disconnect;
		}
		rc = local_exec_container(rpm, name, &config);
		if (rc) {
			LOG(ERROR, "Exec of container %s failed: %s\n",
				rpm, strerror(rc));
			goto out_disconnect;
		}
	case OP_MODE_NODE:
		rc = enter_mode_loop(api, &config);
		if (rc) {
			LOG(ERROR, "Mode operation terminated abnormally: %s\n",
				strerror(rc));
			goto out_disconnect;
		}
		break;
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

