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

void release_configuration(struct agent_config *config)
{
	free(config->db);
	free(config->node);
	free(config->master);
}

int read_configuration(char *config_path, struct agent_config *acfg)
{
	struct config_t config
	int rc;
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
			config_error_test(&config));
		goto out;
	}

	rc = parse_db_config(&config, &acfg->db);
	if (rc)
		goto out;

out:
	config_destroy(&config);
	release_configuration(config
	return 0;
}

