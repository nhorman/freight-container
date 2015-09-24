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
#include <xmlrpc-c/abyss.h>

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
	int i;
	
	LOG(DEBUG, "Got a get_table request\n");

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
		if (!strncmp(tablenames[i], tableval, strlen(tablenames[i]))) {
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

	LOG(DEBUG, "DONE WITH GET GET TABLE %p\n", table);	
	free_tbl(table);
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

extern void handle_freight_rpc(TSession *sessionP, TRequestInfo *requestP,
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
			return idx->handler(sessionP, requestP, acfg);
		}
		idx++;
	}
}

