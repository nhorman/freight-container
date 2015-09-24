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
#include <freight-log.h>
#include <freight-config.h>
#include <freight-db.h>
#include <xmlrpc-c/abyss.h>


void get_table(TSession *sessionP, TRequestInfo *requestP)
{
	LOG(DEBUG, "Got a get_table request\n");
}







struct handler_entry {
	const char *uri;
	void (*handler)(TSession *sessionP, TRequestInfo *requestP);
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
			       abyss_bool * const handledP)
{
	struct handler_entry *idx;

	idx = &handlers[0];
	*handledP = FALSE;

	while (idx->uri) {
		if (!strncmp(requestP->uri, idx->uri, strlen(requestP->uri))) {
			*handledP = TRUE;
			ResponseStatus(sessionP, 200);
			return idx->handler(sessionP, requestP);
		}
		idx++;
	}
}

