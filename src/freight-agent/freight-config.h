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
 * *File: freight-config.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description: Configur structures for freight-agent 
 * *********************************************************/


#ifndef _FREIGHT_CONFIG_H_
#define _FREIGHT_CONFIG_H_

struct db_config {
	char *hostaddr;
	char *dbname;
	char *user;
	char *password;
};

struct node_config {
	char *container_root;
};

struct master_config {
	/* empty for now */
};
	
struct agent_config {
	struct db_config db;
	struct node_config node;
	struct master_config master;	
};


int read_configuration(char *config_path, struct agent_config *config);

void release_configuration(struct agent_config *config);

#endif
