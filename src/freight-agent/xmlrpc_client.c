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
			table->value[i][j] = strdup(tmps);
			xmlrpc_DECREF(tmpc);
		}
		xmlrpc_DECREF(tmpr);
	}

	xmlrpc_DECREF(result);

	return table;
}

static xmlrpc_value* get_add_repo_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *name, *url;
	char stor;


	params = xmlrpc_array_new(&info->env);

	name = strstr(sql, "'");
	name += 1;
	tmp = strstr(name, "'");
	stor = *tmp;
	*tmp = 0;

	p = xmlrpc_string_new(&info->env, name);	

	*tmp = stor;

	xmlrpc_array_append_item(&info->env, params, p);

	xmlrpc_DECREF(p);

	url = strstr(tmp+1, "'");
	url += 1;
	tmp = strstr(url, "'");
	stor = *tmp;
	*tmp = 0;

	p = xmlrpc_string_new(&info->env, url);

	xmlrpc_array_append_item(&info->env, params, p);

	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
	
}

static xmlrpc_value* get_del_repo_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *name;
	char stor;


	params = xmlrpc_array_new(&info->env);

	name = strstr(sql, "'");
	name += 1;
	tmp = strstr(name, "'");
	stor = *tmp;
	*tmp = 0;

	p = xmlrpc_string_new(&info->env, name);	

	*tmp = stor;

	xmlrpc_array_append_item(&info->env, params, p);

	xmlrpc_DECREF(p);

	return params;
	
}

static xmlrpc_value* get_container_create_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Skip over TENNANT PARAM, its implied by auth header */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* Get CNAME param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);	
	*tmp = stor;
	xmlrpc_array_append_item(&info->env, params, p);
	xmlrpc_DECREF(p);

	/* Get INAME param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	/* HOST PARAM */
	value = strstr(tmp+1, "'");
	if (value == NULL) {
		/*
		 * This is an optional parameter
		 * Just add a marker string for the server
		 */
		p = xmlrpc_string_new(&info->env, "scheduler-chosen");
		xmlrpc_array_append_item(&info->env, params, p);
		goto done;
	}
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
done:
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_container_del_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Skip over the TENNANT param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* Get INAME param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_boot_container_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Get NEWSTATE param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	if (strcmp(value, "start-requested")) {
		*tmp = stor;
		/* This isn't a boot request */
		return NULL;
	}

	*tmp = stor;

	/* Skip the TENNANT param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* Get the INAME param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_poweroff_container_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Get NEWSTATE param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	if (strcmp(value, "exiting")) {
		*tmp = stor;
		/* This isn't a poweroff request */
		return NULL;
	}


	*tmp = stor;

	/* Skip the TENNANT param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* Get the INAME param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_network_create_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Get NAME param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);	
	*tmp = stor;
	xmlrpc_array_append_item(&info->env, params, p);
	xmlrpc_DECREF(p);

	/* skip TENNANT param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* skip STATE PARAM */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* get CONFIG param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_network_delete_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Get NAME param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);	
	*tmp = stor;
	xmlrpc_array_append_item(&info->env, params, p);
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_network_container_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* Skip over the TENNANT param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");

	/* get CONTAINER param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	/* get NETWORK param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
}

static xmlrpc_value* get_update_config_params(const char *sql, const struct agent_config *acfg)
{
	struct xmlrpc_info *info = acfg->db.db_priv;
	xmlrpc_value *params;
	xmlrpc_value *p;
	char *tmp;
	char *value;
	char stor;


	params = xmlrpc_array_new(&info->env);

	/* get KEY param */
	value = strstr(sql, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	/* get VALUE param */
	value = strstr(tmp+1, "'");
	value += 1;
	tmp = strstr(value, "'");
	stor = *tmp;
	*tmp = 0;
	p = xmlrpc_string_new(&info->env, value);
	xmlrpc_array_append_item(&info->env, params, p);
	*tmp = stor;
	xmlrpc_DECREF(p);

	return params;
}

static int parse_int_result(xmlrpc_value *result, const struct agent_config *acfg)
{
	int rc;
	struct xmlrpc_info *info = acfg->db.db_priv;

	xmlrpc_decompose_value(&info->env, result, "i", &rc);

	return rc;
}


/*
 * Parse table for xmlrpc_clent.
 * This set of structure tables lets us figure out what to do based on the
 * passed in sql on the send_raw_sql command
 */

struct xmlrpc_ops {
	char *table;
	char *xmlrpc_op;
	xmlrpc_value* (*get_params)(const char *sql, const struct agent_config *acfg);
	int (*parse_result)(xmlrpc_value *result, const struct agent_config *acfg);
};

static struct xmlrpc_ops insert_ops[] = {
	{"yum_config", "add.repo", get_add_repo_params, parse_int_result},
	{"containers", "create.container", get_container_create_params, parse_int_result},
	{"networks", "create.network", get_network_create_params, parse_int_result},
	{"net_container_map", "attach.network", get_network_container_params, parse_int_result},
	{NULL, NULL, NULL, NULL},
};

static struct xmlrpc_ops delete_ops[] = {
	{"yum_config", "del.repo", get_del_repo_params, parse_int_result},
	{"containers", "del.container", get_container_del_params, parse_int_result},
	{"networks", "delete.network", get_network_delete_params, parse_int_result},
	{"net_container_map", "detach.network", get_network_container_params, parse_int_result},
	{NULL, NULL, NULL, NULL},
};

static struct xmlrpc_ops update_ops[] = {
	{"containers", "boot.container", get_boot_container_params, parse_int_result},
	{"containers", "poweroff.container", get_poweroff_container_params, parse_int_result},
	{"global_config", "update.config", get_update_config_params, parse_int_result},
	{NULL, NULL, NULL, NULL},
};

static int run_ops(const char *values, struct xmlrpc_ops *ops,
		   const struct agent_config *acfg)
{
	struct xmlrpc_ops *xop;
	xmlrpc_value *params;
	char *sql;
	xmlrpc_value *result;
	int rc;
	struct xmlrpc_info *info = acfg->db.db_priv;

	sql = NULL;
	for (xop = &ops[0]; xop->table; xop++) {
try_again:
		if (!xop->table)
			break;
		if (!strncmp(values, xop->table, strlen(xop->table))) {
			sql = strstr(values, xop->table);
			sql += strlen(xop->table)+1;
			break;
		}
	}

	if (!xop->table)
		return -EOPNOTSUPP;

	params = xop->get_params(sql, acfg);

	if (!params) {
		xop++;
		goto try_again;
	}

	xmlrpc_client_call2(&info->env, info->client,
			    info->server, xop->xmlrpc_op,
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

	rc = xop->parse_result(result, acfg);

	xmlrpc_DECREF(result);
	return rc;
}

static int insert_fn(const char *values, const struct agent_config *acfg)
{
	char *table;

	table = strstr(values, "INTO");

	if (!table)
		return -EINVAL;

	table += strlen("INTO") + 1;

	return run_ops(table, insert_ops, acfg);
}

static int delete_fn(const char *values, const struct agent_config *acfg)
{
	char *table;

	table = strstr(values, "FROM");

	if (!table)
		return -EINVAL;

	table += strlen("FROM") + 1;

	return run_ops(table, delete_ops, acfg);
}

static int update_fn(const char *values, const struct agent_config *acfg)
{
	return run_ops(values, update_ops, acfg);
}

static int notify_fn(const char *values, const struct agent_config *acfg)
{
	/* Just pretend like this worked */
	return 0;
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

static xmlrpc_value *report_unsupported(enum table_op op, enum db_table tbl,
						 const struct colvallist *setlist,
						 const struct colvallist *filter,
						 char **xmlop, const struct agent_config *acfg)
{
	*xmlop = NULL;
	LOG(ERROR, "the xmlrpc client cannot add hosts to the cluster at this time\n");
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
	[TABLE_TENNANT_HOSTS][OP_INSERT] = {report_unsupported, NULL},
	[TABLE_TENNANT_HOSTS][OP_DELETE] = {report_unsupported, NULL},
	[TABLE_CONTAINERS][OP_INSERT] = {insert_containers_op, parse_int_result},
	[TABLE_CONTAINERS][OP_DELETE] = {delete_containers_op, parse_int_result},
	[TABLE_NETWORKS][OP_INSERT] = {insert_networks_op, parse_int_result},
	[TABLE_NETMAP][OP_INSERT] = {insert_netmap_op, parse_int_result},
	[TABLE_NETMAP][OP_DELETE] = {delete_netmap_op, parse_int_result},
};


static int xmlrpc_table_op(enum table_op op, enum db_table tbl, const struct colvallist *setlist,
			   const struct colvallist *filter,  const struct agent_config *acfg)
{
	int rc;
	xmlrpc_value *params, *result;
	char *xmlop;
	struct xmlrpc_info *info = acfg->db.db_priv;

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

	struct sql_op_fns {
		const char *op;
		int (*opfn)(const char *values, const struct agent_config *acfg);
	} sql_ops[] = {
		{"INSERT", insert_fn},
		{"UPDATE", update_fn},
		{"DELETE", delete_fn},
		{"NOTIFY", notify_fn},
		{NULL,NULL},
	};
	struct sql_op_fns *sql_op;

	int rc = -EOPNOTSUPP;


	for(sql_op = &sql_ops[0]; sql_op->op; sql_op++) {
		if (!strncmp(values, sql_op->op, strlen(sql_op->op))) {
			char *sql = strstr(values, sql_op->op);
			sql += strlen(sql_op->op) + 1;
			rc = sql_op->opfn(sql, acfg);
			break;
		}
	}

	return rc;
}

struct db_api xmlrpc_api = {
	.init = xmlrpc_init,
	.cleanup = xmlrpc_cleanup,
	.connect = xmlrpc_connect,
	.disconnect = xmlrpc_disconnect,
	.get_table = xmlrpc_get_table,
	.table_op = xmlrpc_table_op,
	.send_raw_sql = xmlrpc_send_raw_sql,
};
