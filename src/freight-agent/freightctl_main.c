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
#include <freight-common.h>
#include <freight-log.h>
#include <freight-config.h>
#include <freight-db.h>

struct agent_config config;


static int config_op(char **argv, int argc,
		     const struct agent_config *acfg)
{
	int rc = -EINVAL;
	struct tbl *cfgs;
	struct config_setting *cfg = NULL;
	int i;

	if (argc < 1)
		goto out;

	if (!strcmp(argv[0], "show")) {
		cfgs = get_global_config(acfg);
		printf("%-20s%-20s\n", "KEY", "VALUE");
		printf("____________________|____________________\n");
		for (i = 0; cfgs && i < cfgs->rows; i++) {
			printf("%-20s|%-20s\n", (char *)lookup_tbl(cfgs, i, COL_NAME),
					    (char *)lookup_tbl(cfgs, i, COL_CONFIG));
		}
		free_tbl(cfgs);
		rc = 0;
		goto out;

	} else if (!strcmp(argv[0], "set")) {
		if (argc < 3)
			return -EINVAL;
		cfg = alloc_config_setting(argv[1]);
		if (!cfg) {
			LOG(ERROR, "No such key: %s\n", argv[1]);
			goto out;
		}

		switch(cfg->type) {
		case INT_TYPE:
			*cfg->val.intval = strtol(argv[2], NULL, 10);
			break;
		default:
			LOG(ERROR, "Unknown type!\n");
			goto out;
		}

		if (set_global_config_setting(cfg, acfg)) {
			LOG(ERROR, "Unable to set config key\n");
			goto out;
		}
		rc = 0;
	}
		
out:
	free_config_setting(cfg);
	return rc;
}

/*
 * The repo operation takes 5 arguments
 * <add|del> - add or remove a repo
 * <name> - The name of the repo to add/remove
 * <url> - the url of the repo (for an add operation)
 */
static int repo_op(char **argv, int argc,
		   const struct agent_config *acfg,
		   const struct db_api *api)
{
	int rc = -EINVAL;

	if (argc < 2)
		goto out;

	if (!strcmp(argv[0], "add")) {
		LOG(INFO, "Adding repository %s\n", argv[1]);
		rc = add_repo(argv[1], argv[2], acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "del")) {
		LOG(INFO, "Deleteing repository %s\n", argv[1]);
		rc = del_repo(argv[1], acfg->db.user, acfg);
	} else
		rc = -EINVAL;
out:
	return rc;	
}

/*
 * The host operation takes 5 arguments
 * <add|del> - add or remove a repo
 * <host> - The name of the host to add/remove
 */
static int host_op(char **argv, int argc,
		   const struct agent_config *acfg,
		   const struct db_api *api)
{
	int rc = -EINVAL;

	if (argc < 2)
		goto out;

	if (!strcmp(argv[0], "add")) {
		LOG(INFO, "Adding host %s\n", argv[1]);
		rc = add_host(argv[1], acfg);
	} else if (!strcmp(argv[0], "del")) {
		LOG(INFO, "Deleteing host %s\n", argv[1]);
		rc = del_host(argv[1], acfg);
	} else if (!strcmp(argv[0], "subscribe")) {
		LOG(INFO, "Subscribing host %s to tennant %s\n",
			argv[1], argv[2]);
		rc = subscribe_host(argv[1], argv[2], acfg);
	} else if (!strcmp(argv[0], "unsubscribe")) {
		LOG(INFO, "Unsubscribing host %s from tennant %s\n",
			argv[1], argv[2]);
		rc = unsubscribe_host(argv[2], argv[1], acfg);
	} else if (!strcmp(argv[0], "list")) {
		rc = list_subscriptions(argv[1], acfg);
	} else
		rc = -EINVAL;
out:
	return rc;	
}

static int container_op(char **argv, int argc,
		const struct agent_config *acfg,
		const struct db_api *api)
{
	int rc = -EINVAL;
	char *host;

	if (!strcmp(argv[0], "create")) {
		if (argc < 3)
			goto out;
		if (argc > 3)
			host = argv[3];
		else
			host = NULL;

		LOG(INFO, "Issuing create container %s on %s\n", argv[2], host ? host : "any");
		rc = request_create_container(argv[1], argv[2], host, acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "delete")) {
		int force = 0;
		if (argc < 3)
			goto out;
		LOG(INFO, "Issuing delete container %s\n", argv[1]);
		if (argv[2] && !strcmp(argv[2], "force"))
			force = 1;
		rc = request_delete_container(argv[1], acfg->db.user, force, acfg);
	} else if (!strcmp(argv[0], "boot")) {
		if (argc < 2)
			goto out;
		LOG(INFO, "Booting Container %s\n", argv[1]);
		rc = request_boot_container(argv[1], acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "poweroff")) {
		if (argc < 2)
			goto out;
		LOG(INFO, "Powering off container %s\n", argv[1]);
		rc = request_poweroff_container(argv[1], acfg->db.user, acfg);

	} else if (!strcmp(argv[0], "list")) {
		rc = print_container_list(acfg->db.user, acfg);
		if (rc == -ENOENT)
			LOG(INFO, "No Containers\n");
		rc = 0;
	} else
		rc = -EINVAL;
out:
	return rc;
}

static int network_op(char **argv, int argc,
		const struct agent_config *acfg,
		const struct db_api *api)
{

	int rc = -EINVAL;

	if (!strcmp(argv[0], "create")) {
		LOG(INFO, "Creating network %s\n", argv[1]);
		rc = network_create(argv[1], argv[2], acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "delete")) {
		LOG(INFO, "Deleting network %s\n", argv[1]);
		rc = network_delete(argv[1],acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "list")) {
		rc = network_list(acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "attach")) {
		rc = network_attach(argv[1], argv[2], acfg->db.user, acfg);
	} else if (!strcmp(argv[0], "detach")) {
		rc = network_detach(argv[1], argv[2], acfg->db.user, acfg);
	}

	return rc;
}

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
	char *config_file = NULL;
	char *op;
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

	if (!config_file)
		config_file = strjoina(getenv("HOME"), "/.freightctl", NULL);
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

	if (!get_db_api(&config)) {
		LOG(ERROR, "No DB configuration selected\n");
		goto out_release;
	}

	if (db_init(&config)) {
		LOG(ERROR, "Unable to initalize DB subsystem\n");
		goto out_release;
	}

	if (db_connect(&config)) {
		LOG(ERROR, "Could not connect to database\n");
		goto out_cleanup_db;
	}
	

	if (!strcmp(op, "repo")) {
		rc = repo_op(&argv[optind+1], argc-optind, &config, api);
		if (rc)
			LOG(ERROR, "Could not preform repo op: %s\n",
				strerror(rc));
	} else if (!strcmp(op, "host")) { 
		rc = host_op(&argv[optind+1], argc-optind, &config, api);
		if (rc)
			LOG(ERROR, "Could not preform host op: %s\n",
				strerror(rc));
	} else if (!strcmp(op, "container")) {
		rc = container_op(&argv[optind+1], argc-optind, &config, api);
		if (rc)
			LOG(ERROR, "Could not preform container op: %s\n",
				strerror(rc));
	} else if (!strcmp(op, "network")) {
		rc = network_op(&argv[optind+1], argc-optind, &config, api);
		if (rc)
			LOG(ERROR, "Could not preform network op: %s\n",
				strerror(rc));
	} else if (!strcmp(op, "config")) {
		rc = config_op(&argv[optind+1], argc-optind, &config);
		if (rc)
			LOG(ERROR, "Could not preform config op: %s\n",
				strerror(rc));
	} else {
		LOG(ERROR, "Unknown operation\n");
		goto out_disconnect;
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

