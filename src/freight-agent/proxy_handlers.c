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
#include <proxy_handlers.h>

xmlrpc_value* get_table(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo)
{
	char *tablearg, *tablename;
	struct tbl *table;
	char *filter;
	enum db_table tid;
	int i, j;
	xmlrpc_value *xtbl;
	xmlrpc_value *xrow;
	xmlrpc_value *xcell;
	const struct agent_config *acfg = serverinfo;
	const struct call_info *cinfo = callinfo;

	xmlrpc_parse_value(envp, params, "(s)", &tablearg);
	tablename = strchr(tablearg, '=');
	if(!tablename)
		return xmlrpc_nil_new(envp); 

	tid = get_tableid(tablename+1);


	if (!tid)
		return xmlrpc_nil_new(envp);

	filter = strjoina("tennant='",cinfo->tennant,"'",NULL);
	table = get_raw_table(tid, filter, acfg);

	if (!table)
		return xmlrpc_nil_new(envp);

	xtbl = xmlrpc_array_new(envp);

	for (i = 0; i < table->rows; i++) {
		xrow = xmlrpc_array_new(envp);
		for (j=0; j < table->cols; j++) {
			xcell = xmlrpc_string_new(envp, (char *)table->value[i][j]);
			xmlrpc_array_append_item(envp, xrow, xcell);
			xmlrpc_DECREF(xcell);
		}
		xmlrpc_array_append_item(envp, xtbl, xrow);
		xmlrpc_DECREF(xrow);
	}

	/*
	 * free everything
	 */
	free_tbl(table);

	return xtbl;
}


xmlrpc_value* xmlrpc_add_repo(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo)
{
	char *rname, *rurl;
	const struct agent_config *acfg = serverinfo;
	const struct call_info *cinfo = callinfo;
	int rc;

	xmlrpc_decompose_value(envp, params, "(ss)", &rname, &rurl);

	rname = strstr(rname, "=");
	rname += 1;

	rurl = strstr(rurl, "=");
	rurl += 1;

	rc = add_repo(rname, rurl, cinfo->tennant, acfg);

	return xmlrpc_int_new(envp, rc);
}

xmlrpc_value* xmlrpc_del_repo(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo)
{
	char *rname;
	const struct agent_config *acfg = serverinfo;
	const struct call_info *cinfo = callinfo;
	int rc;

	xmlrpc_decompose_value(envp, params, "(s)", &rname);

	rname = strstr(rname, "=");
	rname += 1;

	rc = del_repo(rname, cinfo->tennant, acfg);

	return xmlrpc_int_new(envp, rc);
}

xmlrpc_value* xmlrpc_create_container(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo)
{
	char *cname, *iname, *host;
	const struct agent_config *acfg = serverinfo;
	const struct call_info *cinfo = callinfo;
	int rc;


	xmlrpc_decompose_value(envp, params, "(sss)", &iname, &cname, &host);

	LOG(DEBUG, "CREATING CONTAINER %s: %s on host %s\n", cname, iname, host);
	rc = request_create_container(cname, iname, host, cinfo->tennant, acfg);

	return xmlrpc_int_new(envp, rc);	
}
