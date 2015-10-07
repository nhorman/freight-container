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
 *File: xmlrpc_client.c 
 *
 *Author:Neil Horman
 *
 *Date:
 *
 *Description implements access to a freightproxy based db server
 *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <freight-common.h>
#include <freight-log.h>
#include <freight-db.h>
#include <xmlrpc-c/client.h>

struct xmlrpc_info {
	xmlrpc_env env;
	xmlrpc_client *client;
	xmlrpc_server_info *server;
};

static int xmlrpc_init(struct agent_config *acfg)
{
	struct xmlrpc_info *info;
	char *port = "80";
	char *baseurl;

	acfg->db.db_priv = info = calloc(1, sizeof(struct xmlrpc_info));
	if (!acfg->db.db_priv)
		return -ENOMEM;

	xmlrpc_env_init(&info->env);
	xmlrpc_client_setup_global_const(&info->env);
	xmlrpc_client_create(&info->env, XMLRPC_CLIENT_NO_FLAGS, "Freight Proxy Client",
			     "1.0", NULL, 0, &info->client);
	if (acfg->db.hostport)
		port = acfg->db.hostport;

	baseurl = strjoina("http://",acfg->db.hostaddr,":",port,"/", NULL);

	info->server = xmlrpc_server_info_new(&info->env, baseurl);

	/*
	 * Set the auth header
	 */
	xmlrpc_server_info_set_user(&info->env, info->server,
				    acfg->db.user, acfg->db.password);
	xmlrpc_server_info_allow_auth_basic(&info->env, info->server);

	return 0;
}

static void xmlrpc_cleanup(struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;

	xmlrpc_server_info_free(info->server);
	xmlrpc_client_destroy(info->client);
	xmlrpc_client_teardown_global_const();
	xmlrpc_env_clean(&info->env);

	free(info);
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
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *result;
	char *tablearg;
	const char *tmps;
	xmlrpc_value *tablename;
	xmlrpc_value *params;
	xmlrpc_value *tmpr, *tmpc;
	int r, c, i, j;
	struct tbl *table;

	params = xmlrpc_array_new(&info->env);

	tablearg = strjoina("table=",get_tablename(type));
	tablename = xmlrpc_string_new(&info->env, tablearg); 
	xmlrpc_array_append_item(&info->env, params, tablename);
	xmlrpc_DECREF(tablename);

	xmlrpc_client_call2(&info->env, info->client,
			    info->server, "get.table",
			    params,
			    &result);

	xmlrpc_DECREF(params);

	/*
	 * check to make sure it worked properly
	 */
	if (info->env.fault_occurred) {
		LOG(ERROR, "Failed to issue xmlrpc: %s\n", info->env.fault_string);
		return NULL;
	}

	/*
	 * Now we find out how many rows and colums this table has
	 */
	r = xmlrpc_array_size(&info->env, result);
	if (!r)
		c = 0;
	else {
		xmlrpc_array_read_item(&info->env, result, 0, &tmpr);
		c = xmlrpc_array_size(&info->env, tmpr); 
		xmlrpc_DECREF(tmpr);
	}

	LOG(DEBUG, "Allocating table of %d rows by %d cols\n", r, c);
	table = alloc_tbl(r, c, type);

	for (i=0; i < r; i++) {
		xmlrpc_array_read_item(&info->env, result, i, &tmpr);

		for (j=0; j < c; j++) {
			xmlrpc_array_read_item(&info->env, tmpr, j, &tmpc);
			xmlrpc_read_string(&info->env, tmpc, &tmps);
			LOG(DEBUG, "INSERTING %s to %d:%d\n", tmps, i, j);
			table->value[i][j] = strdup(tmps);
			xmlrpc_DECREF(tmpc);
		}
		xmlrpc_DECREF(tmpr);
	}

	xmlrpc_DECREF(result);

	return table;
}

struct db_api xmlrpc_api = {
	.init = xmlrpc_init,
	.cleanup = xmlrpc_cleanup,
	.connect = xmlrpc_connect,
	.disconnect = xmlrpc_disconnect,
	.get_table = xmlrpc_get_table,
};
