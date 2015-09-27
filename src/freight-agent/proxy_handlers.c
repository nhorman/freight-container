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
 *File: freightctl_main.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description
 *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <config.h>
#include <freight-common.h>
#include <freight-log.h>
#include <freight-config.h>
#include <freight-db.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/util.h>

static const char *tablenames[] = {
 [TABLE_TENNANTS] = "tennants",
 [TABLE_NODES] = "nodes",
 [TABLE_TENNANT_HOSTS] = "tennant_hosts",
 [TABLE_YUM_CONFIG] = "yum_config",
 [TABLE_CONTAINERS] = "containers",
 NULL
};

void get_table(TSession *sessionP, TRequestInfo *requestP, const struct agent_config *acfg)
{
	const char *query = requestP->query;
	const char *tablearg, *tableval;
	struct tbl *table;
	char *filter;
	enum db_table tid;
	int i, j;
	xmlrpc_env env;
	xmlrpc_value *xtbl;
	xmlrpc_value *xrow;
	xmlrpc_value *xcell;
	xmlrpc_mem_block *output;

	if (!query) {
		ResponseStatus(sessionP, 500);
		ResponseError2(sessionP, "Need to specify a table to get");
		return;
	}

	tablearg = strstr(query, "table");
	if (!tablearg) {
		ResponseStatus(sessionP, 500);
		ResponseError2(sessionP, "Need to specify a table parameter");
		return;
	}

	tableval = strchr(tablearg, '=');
	if (!tableval) {
		ResponseStatus(sessionP, 500);
		ResponseError2(sessionP, "No table specified");
		return;
	}

	/*
	 * Advance past the '=' to the real value
	 */
	tableval++;
	tid = TABLE_MAX;

	for (i=0; tablenames[i] != NULL; i++) {
		if (!strncmp(tablenames[i], tableval, 128)) {
			tid = i;
			break;
		}
	}

	if (tid == TABLE_MAX) {
		ResponseStatus(sessionP, 500);
		ResponseError2(sessionP, "No such table\n");
		return;
	}

	filter = strjoina("tennant='",requestP->user,"'",NULL);
	table = get_raw_table(tid, filter, acfg);

	if (!table){
		ResponseStatus(sessionP, 500);
		ResponseError2(sessionP, "Failed to get table");
		return;
	}

	xmlrpc_env_init(&env);
	xtbl = xmlrpc_array_new(&env);

	for (i = 0; i < table->rows; i++) {
		xrow = xmlrpc_array_new(&env);
		for (j=0; j < table->cols; j++) {
			xcell = xmlrpc_string_new(&env, (char *)table->value[i][j]);
			xmlrpc_array_append_item(&env, xrow, xcell);
			xmlrpc_DECREF(xcell);
		}
		xmlrpc_array_append_item(&env, xtbl, xrow);
		xmlrpc_DECREF(xrow);
	}


	output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);

	xmlrpc_serialize_response(&env, output, xtbl);

	ResponseWriteBody(sessionP, xmlrpc_mem_block_contents(output),
			  xmlrpc_mem_block_size(output));

	/*
	 * free everything
	 */
	xmlrpc_DECREF(xtbl);
	free_tbl(table);
	XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
	xmlrpc_env_clean(&env);
}







struct handler_entry {
	const char *uri;
	void (*handler)(TSession *sessionP, TRequestInfo *requestP, const struct agent_config *acfg);
} handlers[] = {
	{
		"/get.table",
		get_table
	},
	{
		NULL,
		NULL,
	},
};

void handle_freight_rpc(TSession *sessionP, TRequestInfo *requestP,
			       abyss_bool * const handledP,
			       const struct agent_config *acfg)
{
	struct handler_entry *idx;

	idx = &handlers[0];
	*handledP = FALSE;

	while (idx->uri) {
		if (!strncmp(requestP->uri, idx->uri, strlen(requestP->uri))) {
			*handledP = TRUE;
			ResponseStatus(sessionP, 200);
			idx->handler(sessionP, requestP, acfg);
		}
		idx++;
	}

}

