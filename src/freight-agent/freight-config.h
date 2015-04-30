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


enum db_type {
	DB_TYPE_NONE = 0,
	DB_TYPE_POSTGRES,
};


struct db_config {
	enum db_type dbtype;
	char *hostaddr;
	char *dbname;
	char *user;
	char *password;
	void *db_priv;
};

struct node_config {
	char *container_root;
};

struct master_config {
	/* empty for now */
};

enum op_mode {
	OP_MODE_NODE = 0,
	OP_MODE_INIT,
	OP_MODE_CLEAN,
	OP_MODE_INSTALL,
	OP_MODE_UNINSTALL, 
	OP_MODE_LIST,
	OP_MODE_EXEC,
};

struct cmdline_config {
	enum op_mode mode;
	int verbose;
};

struct agent_config {
	struct db_config db;
	struct node_config node;
	struct master_config master;	
	struct cmdline_config cmdline;
};


int read_configuration(char *config_path, struct agent_config *config);

void release_configuration(struct agent_config *config);

#endif
