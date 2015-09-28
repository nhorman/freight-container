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
 * *File: xmlrpc_client.c 
 * *
 * *Author:Neil Horman
 * *
 * *Date:
 * *
 * *Description implements access to a freightproxy based db server
 * *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <freight-log.h>
#include <freight-db.h>
#include <xmlrpc-c/client.h>

struct xmlrpc_info {
	xmlrpc_env env;
	xmlrpc_client *client;
	char *baseurl;
};

static int xmlrpc_init(struct agent_config *acfg)
{
	struct xmlrpc_info *info;
	acfg->db.db_priv = info = calloc(1, sizeof(struct xmlrpc_info));
	if (!acfg->db.db_priv)
		return -ENOMEM;

	xmlrpc_env_init(&info->env);
	xmlrpc_client_setup_global_const(&info->env);
	xmlrpc_client_create(&info->env, XMLRPC_CLIENT_NO_FLAGS, "Freight Proxy Client",
			     "1.0", NULL, 0, &info->client);
	return 0;
}

static void xmlrpc_cleanup(struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;

	xmlrpc_client_destroy(info->client);
	xmlrpc_client_teardown_global_const();
	xmlrpc_env_clean(&info->env);

	free(acfg->db.db_priv);
	acfg->db.db_priv = NULL;
	return;
}

static int xmlrpc_disconnect(struct agent_config *acfg)
{
	return 0;
}

static int xmlrpc_connect(struct agent_config *acfg)
{
	return 0;
}

struct tbl* xmlrpc_get_table(enum db_table type, const char *cols, const char *filter,
                                 const struct agent_config *acfg)
{
	struct tbl *table = NULL;

	/*
 	 * For this local dummy database, we always just return a local tennant 
 	 * From the tennant_hosts table
 	 */
	if (type == TABLE_TENNANT_HOSTS) {
		table = alloc_tbl(1,2, type);
		if (table)
			table->value[0][1] = strdup("local");
	}

	return table;
}

struct db_api xmlrpc_api = {
	.init = xmlrpc_init,
	.cleanup = xmlrpc_cleanup,
	.connect = xmlrpc_connect,
	.disconnect = xmlrpc_disconnect,
	.get_table = xmlrpc_get_table,
};
