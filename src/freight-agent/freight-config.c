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

int read_configuration(char *config_path, struct agent_config *acfg)
{
	struct config_t config;
#if 0
	int rc;
#endif
	struct stat buf;

	memset(acfg, 0, sizeof(struct agent_config));

	config_init(&config);

	if (stat(config_path, &buf)) {
		LOG(ERROR, "Config path does not exist\n");
		goto out;
	}

	if (config_read_file(&config, config_path) == CONFIG_FALSE) {
		LOG(ERROR, "Error in %s:%d : %s\n",
			config_error_file(&config),
			config_error_line(&config),
			config_error_text(&config));
		goto out;
	}
#if 0
	rc = parse_db_config(&config, &acfg->db);
	if (rc)
		goto out;
#endif

out:
	config_destroy(&config);
	return 0;
}

