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
#include <freight-config.h>
#include <freight-common.h>

void release_configuration(struct agent_config *config)
{
	if (!config)
		return;
	
	free(config->db.hostaddr);
	free(config->db.dbname);
	free(config->db.user);
	free(config->db.password);
	free(config->node.container_root);
	free(config->proxy.logpath);
}

static int parse_entry(config_setting_t *config, char **b, const char *name) {
	config_setting_t *t;
	char *p;

	t = config_setting_get_member(config, name);
	if (!t)
		return -ENOENT;

	p = strdup(config_setting_get_string(t));
	if (!p)
		return -ENOMEM;

	*b = p;

	return 0;
}

static int parse_db_config(config_t *cfg, struct db_config *db)
{
	int rc = -EINVAL;
	config_setting_t *db_cfg = config_lookup(cfg, "db");
	config_setting_t *tmp;

	if (!db_cfg) {
		db->dbtype = DB_TYPE_NONE;
		rc = 0;
		goto out;
	}

	tmp = config_setting_get_member(db_cfg, "dbtype");
	if (!tmp) {
		LOG(ERROR, "db config requires a type\n");
		goto out;
	}

	if (streq(config_setting_get_string(tmp), "postgres"))
		db->dbtype = DB_TYPE_POSTGRES;
	else {
		LOG(ERROR, "Unknown DB type\n");
		goto out;
	}

	rc = 0;

	/*
 	 * hostaddr, dbname, user and pass are all optional based on type
 	 */
	rc = parse_entry(db_cfg, &db->hostaddr, "hostaddr");
	if (rc < 0)
		return rc;

	rc = parse_entry(db_cfg, &db->dbname, "dbname");
	if (rc < 0)
		return rc;

	rc = parse_entry(db_cfg, &db->user, "user");
	if (rc < 0)
		return rc;

	rc = parse_entry(db_cfg, &db->password, "password");
	if (rc < 0)
		return rc;

out:
	return rc;
}

static int parse_node_config(config_t *cfg, struct node_config *node)
{
	int rc = 0;
	config_setting_t *node_cfg = config_lookup(cfg, "node");

	/*
 	 * Not having a node config isn't fatal
 	 */
	if (!node_cfg)
		goto out;
	
	rc = parse_entry(node_cfg, &node->container_root, "container_root");
	if (rc == -ENOENT) 
		LOG(ERROR, "node config must contain a valid container_root");
	
out:
	return rc;
}

static int parse_proxy_config(config_t *cfg, struct proxy_config *proxy)
{
	int rc = 0;
	config_setting_t *proxy_cfg = config_lookup(cfg, "proxy");
	char *serverport;

	memset(proxy, 0, sizeof(struct proxy_config));

	/*
 	 * Not having a proxy config isn't fatal
 	 */
	if (!proxy_cfg)
		goto out;
	
	rc = parse_entry(proxy_cfg, &serverport, "serverport");
	if (rc == -ENOENT) 
		LOG(ERROR, "proxy config must contain a valid serverport");
	else
		proxy->serverport = strtoul(serverport, NULL, 10);

	free(serverport);

	/* NULL entry is ok for log */
	parse_entry(proxy_cfg, &proxy->logpath, "log");
	
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

	rc = parse_proxy_config(&config, &acfg->proxy);
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

