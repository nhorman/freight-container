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

static xmlrpc_value *report_unsupported(enum table_op op, enum db_table tbl,
                                                 const struct colvallist *setlist,
                                                 const struct colvallist *filter,
                                                 char **xmlop, const struct agent_config *acfg);

static int xmlrpc_init_driver(struct agent_config *acfg)
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
	if (r <= 0)
		c = 0;
	else {
		xmlrpc_array_read_item(&info->env, result, 0, &tmpr);
		c = xmlrpc_array_size(&info->env, tmpr); 
		xmlrpc_DECREF(tmpr);
	}

	table = alloc_tbl(r, c, type);

	for (i=0; i < r; i++) {
		xmlrpc_array_read_item(&info->env, result, i, &tmpr);

		for (j=0; j < c; j++) {
			xmlrpc_array_read_item(&info->env, tmpr, j, &tmpc);
			xmlrpc_read_string(&info->env, tmpc, &tmps);
			if (tmps)
				table->value[i][j] = strdup(tmps);
			xmlrpc_DECREF(tmpc);
		}
		xmlrpc_DECREF(tmpr);
	}

	xmlrpc_DECREF(result);

	return table;
}

static int parse_int_result(xmlrpc_value *result, const struct agent_config *acfg)
{
	int rc;
	struct xmlrpc_info *info = acfg->db.db_priv;

	xmlrpc_decompose_value(&info->env, result, "i", &rc);

	return rc;
}


static xmlrpc_value* make_string_array_from_colvallist(const struct colvallist *list, struct xmlrpc_info *info)
{
        xmlrpc_value *params;
        xmlrpc_value *p;
	int i;

        params = xmlrpc_array_new(&info->env);

	for (i=0; i < list->count; i++) {
		p = xmlrpc_string_new(&info->env, list->entries[i].value);
		xmlrpc_array_append_item(&info->env, params, p);
		xmlrpc_DECREF(p);
	}

        return params;
}

static xmlrpc_value *yum_config_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	struct xmlrpc_info *info = acfg->db.db_priv;

	if (op == OP_INSERT)
		*xmlop = "add.repo";
	else
		*xmlop = "del.repo";

	/*
	 * Drop the first entry in this (the tennant) as we don't need it
	 */
	if (op == OP_INSERT) {
		list.count = setlist->count-1;
		list.entries = setlist->entries;
	} else {
		list.count = filter->count-1;
		list.entries = filter->entries;
	}

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *insert_containers_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;

	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "create.container";

	/*
	 * This skips the tennant parameter which the xmlrpc code 
	 * doesn't need
	 */
	list.count = setlist->count-1;
	list.entries = &setlist->entries[1]; 
	if (list.entries[2].value == NULL)
		list.entries[2].value = "scheduler-chosen";

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *delete_containers_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	int i;
	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "del.container";

	/*
	 * This skips the tennant parameter which the xmlrpc code 
	 * doesn't need
	 */
	list.count = 1;
	for (i =0; i < filter->count; i++) {
		if (filter->entries[i].column == COL_INAME)
			break;
	}

	list.entries = &filter->entries[i]; 

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *update_containers_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	struct xmlrpc_info *info = acfg->db.db_priv;

	/*
	 * If the filter addresses anything other than iname,
	 * its a batch update, which the xmlrpc interface doesn't support
	 */
	if (filter->entries[1].column != COL_INAME)
		return report_unsupported(op, tbl, setlist, filter, xmlop, acfg);

	if (!strcmp(setlist->entries[0].value, "start-requested"))
		*xmlop = "boot.container";
	else
		return report_unsupported(op, tbl, setlist, filter, xmlop, acfg);

	/*
	 * This skips the tennant parameter which the xmlrpc code 
	 * doesn't need
	 */
	list.count = 1;
	list.entries = &filter->entries[1];
	
	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *insert_networks_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[2];

	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "create.network";

	/*
	 * This skips the tennant and state parameter which the xmlrpc code 
	 * doesn't need
	 */
	list.count = 2; 
	list.entries = values;
	list.entries[0].column = setlist->entries[0].column;
	list.entries[0].value = setlist->entries[0].value;
	list.entries[1].column = setlist->entries[3].column;
	list.entries[1].value = setlist->entries[3].value;

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *delete_networks_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	int i;

	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "delete.network";

	/*
	 * This skips the tennant and state parameter which the xmlrpc code 
	 * doesn't need
	 */
	list.count = 1; 
	for (i=0; i < filter->count; i++)
		if (filter->entries[i].column == COL_NAME)
			break;
	list.entries = &filter->entries[i];

	return make_string_array_from_colvallist(&list, info);
}
static xmlrpc_value *insert_netmap_op(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;

	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "create.container";

	/*
	 * This skips the tennant parameter which the xmlrpc code 
	 * doesn't need
	 */
	list.count = setlist->count-1;
	list.entries = &setlist->entries[1]; 

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *delete_netmap_op(enum table_op op, enum db_table tbl,
                                                 const struct colvallist *setlist,
                                                 const struct colvallist *filter,
                                                 char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "detach.network";

	list.count = filter->count - 1;
	list.entries = &filter->entries[1];

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *update_config_op(enum table_op op, enum db_table tbl,
				      const struct colvallist *setlist,
				      const struct colvallist *filter,
				      char **xmlop, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval ent[2];
	struct xmlrpc_info *info = acfg->db.db_priv;

	*xmlop = "update.config";

	list.count = 2;
	list.entries = ent;

	ent[0].column = setlist->entries[0].column;
	ent[0].value = setlist->entries[0].value;
	ent[1].column = filter->entries[0].column;
	ent[1].value = filter->entries[0].value;

	return make_string_array_from_colvallist(&list, info);
}

static xmlrpc_value *report_unsupported(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	*xmlop = NULL;
	LOG(ERROR, "the xmlrpc client cannot preform this operation at this time\n");
	return NULL;
}

struct xmlrpc_op_map {
	xmlrpc_value *(*get_op_args)(enum table_op op, enum db_table tbl, const struct colvallist *setlist,
			   	     const struct colvallist *filter, char **xmlop, const struct agent_config *acfg);
	int (*parse_result)(xmlrpc_value *result, const struct agent_config *acfg);
};

static struct xmlrpc_op_map op_map[TABLE_MAX][OP_MAX] = {
	[TABLE_YUM_CONFIG][OP_INSERT] = {yum_config_op, parse_int_result},
	[TABLE_YUM_CONFIG][OP_DELETE] = {yum_config_op, parse_int_result},
	[TABLE_NODES][OP_INSERT] = {report_unsupported, NULL},
	[TABLE_NODES][OP_DELETE] = {report_unsupported, NULL},
	[TABLE_NODES][OP_UPDATE] = {report_unsupported, NULL},
	[TABLE_TENNANT_HOSTS][OP_INSERT] = {report_unsupported, NULL},
	[TABLE_TENNANT_HOSTS][OP_DELETE] = {report_unsupported, NULL},
	[TABLE_CONTAINERS][OP_INSERT] = {insert_containers_op, parse_int_result},
	[TABLE_CONTAINERS][OP_DELETE] = {delete_containers_op, parse_int_result},
	[TABLE_CONTAINERS][OP_UPDATE] = {update_containers_op, parse_int_result},
	[TABLE_NETWORKS][OP_INSERT] = {insert_networks_op, parse_int_result},
	[TABLE_NETWORKS][OP_DELETE] = {delete_networks_op, parse_int_result},
	[TABLE_NETMAP][OP_INSERT] = {insert_netmap_op, parse_int_result},
	[TABLE_NETMAP][OP_DELETE] = {delete_netmap_op, parse_int_result},
	[TABLE_GCONF][OP_UPDATE] = {update_config_op, parse_int_result},
};


static int xmlrpc_table_op(enum table_op op, enum db_table tbl, const struct colvallist *setlist,
			   const struct colvallist *filter,  const struct agent_config *acfg)
{
	int rc;
	xmlrpc_value *params, *result;
	char *xmlop;
	struct xmlrpc_info *info = acfg->db.db_priv;

	if (!op_map[tbl][op].get_op_args)
		return -EOPNOTSUPP;

	params = op_map[tbl][op].get_op_args(op, tbl, setlist, filter, &xmlop, acfg);

	if (!params)
		return -EOPNOTSUPP;

	
	xmlrpc_client_call2(&info->env, info->client,
			    info->server, xmlop,
			    params,
			    &result);
	
	xmlrpc_DECREF(params);

	/*
	 * check to make sure it worked properly
	 */
	if (info->env.fault_occurred) {
		LOG(ERROR, "Failed to issue xmlrpc: %s\n", info->env.fault_string);
                return -EFAULT;
	}

	rc = op_map[tbl][op].parse_result(result, acfg);

	xmlrpc_DECREF(result);
	return rc;
}

static int xmlrpc_send_raw_sql(const char *values, const struct agent_config *acfg)
{
	return -EOPNOTSUPP;
}

/*
 * xmlrpc just pretends to notify systems, it really happens on the server
 */
int xmlrpc_null_notify(enum notify_type type, enum listen_channel chn,
                      const char *name, const struct agent_config *acfg)
{
	return 0;
}

struct db_api xmlrpc_api = {
	.init = xmlrpc_init_driver,
	.cleanup = xmlrpc_cleanup,
	.connect = xmlrpc_connect,
	.disconnect = xmlrpc_disconnect,
	.get_table = xmlrpc_get_table,
	.table_op = xmlrpc_table_op,
	.send_raw_sql = xmlrpc_send_raw_sql,
	.notify	= xmlrpc_null_notify,
};
