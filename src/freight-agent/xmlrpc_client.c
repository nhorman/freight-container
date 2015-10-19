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
	{NULL, NULL, NULL, NULL},
};

static struct xmlrpc_ops delete_ops[] = {
	{"yum_config", "del.repo", get_del_repo_params, parse_int_result},
	{"containers", "del.container", get_container_del_params, parse_int_result},
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
		if (!strncmp(values, xop->table, strlen(xop->table))) {
			sql = strstr(values, xop->table);
			sql += strlen(xop->table)+1;
			break;
		}
	}

	if (!xop->table)
		return -EOPNOTSUPP;

	params = xop->get_params(sql, acfg);

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
	return -EOPNOTSUPP;
}

static int notify_fn(const char *values, const struct agent_config *acfg)
{
	/* Just pretend like this worked */
	return 0;
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
	.send_raw_sql = xmlrpc_send_raw_sql,
};
