/*********************************************************
 * *Copyright (C) 2015 Neil Horman
 * *This program is free software; you can redistribute it and\or modify
 * *it under the terms of the GNU General Public License as published 
 * *by the Free Software Foundation; either version 2 of the License,
 * *or  any later version.
 * *
 * *This program is distributed in the hope that it will be useful,
 * *but WITHOUT ANY WARRANTY; without even the implied warranty of
 * *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * *GNU General Public License for more details.
 * *
 * *File: freight-config.c
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description: Configur structures for freight-agent 
 * *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>      
#include <string.h>     
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libconfig.h>
#include <freight-log.h>
#include <freight-config.h>

void release_configuration(struct agent_config *config)
{
	free(config->db.hostaddr);
	free(config->db.dbname);
	free(config->db.user);
	free(config->db.password);
	free(config->node.container_root);
}

static int parse_db_config(config_t *cfg, struct db_config *db)
{
	int rc = -EINVAL;
	config_setting_t *db_cfg = config_lookup(cfg, "db");
	config_setting_t *tmp;

	if (!db_cfg) {
		LOG(ERROR, "freight-agent requires a database configuration\n");
		goto out;
	}

	tmp = config_setting_get_member(db_cfg, "dbtype");
	if (!tmp) {
		LOG(ERROR, "db config requires a type\n");
		goto out;
	}

	if (!strcmp(config_setting_get_string(tmp), "postgres"))
		db->dbtype = DB_TYPE_POSTGRES;
	else {
		LOG(ERROR, "Unknown DB type\n");
		goto out;
	}

	/*
 	 * hostaddr, dbname, user and pass are all optional based on type
 	 */
	tmp = config_setting_get_member(db_cfg, "hostaddr");
	if (tmp)
		db->hostaddr = strdup(config_setting_get_string(tmp));

	tmp = config_setting_get_member(db_cfg, "dbname");
	if (tmp)
		db->dbname = strdup(config_setting_get_string(tmp));

	tmp = config_setting_get_member(db_cfg, "user");
	if (tmp)
		db->user = strdup(config_setting_get_string(tmp));

	tmp = config_setting_get_member(db_cfg, "password");
	if (tmp)
		db->password = strdup(config_setting_get_string(tmp));
	
out:
	return rc;
}

static int parse_node_config(config_t *cfg, struct node_config *node)
{
	int rc = 0;
	config_setting_t *node_cfg = config_lookup(cfg, "node");
	config_setting_t *tmp;

	/*
 	 * Not having a node config isn't fatal
 	 */
	if (!node_cfg)

	tmp = config_setting_get_member(node_cfg, "container_root");
	if (!tmp) {
		rc = -EINVAL;
		LOG(ERROR, "node config must contain a container_root");
		goto out;
	}

	node->container_root = strdup(config_setting_get_string(tmp));
out:
	return rc;
}

int read_configuration(char *config_path, struct agent_config *acfg)
{
	struct config_t config;
	int rc = -EINVAL;
	struct stat buf;

	memset(acfg, 0, sizeof(struct agent_config));

	config_init(&config);

	if (stat(config_path, &buf)) {
		LOG(ERROR, "Config path %s does not exist\n", config_path);
		goto out;
	}

	if (config_read_file(&config, config_path) == CONFIG_FALSE) {
		LOG(ERROR, "Error in %s:%d : %s\n",
			config_error_file(&config),
			config_error_line(&config),
			config_error_text(&config));
		goto out;
	}
	rc = parse_db_config(&config, &acfg->db);
	if (rc)
		goto out_release;

	rc = parse_node_config(&config, &acfg->node);
	if (rc)
		goto out_release;

	/*
 	 * Nothing to parse for the master config...yet
 	 */

out:
	config_destroy(&config);
	return rc;
out_release:
	release_configuration(acfg);
	goto out;
}

