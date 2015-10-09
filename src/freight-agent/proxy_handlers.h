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
 *File: proxy_handlers.h 
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description xmlrpc handlers for requests
 *********************************************************/

#ifndef _PROXY_HANDLERS_H_
#define _PROXY_HANDLERS_H_

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/util.h>

struct call_info {
	const char *tennant;
};

extern xmlrpc_value* get_table(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo);
extern xmlrpc_value* xmlrpc_add_repo(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo);
extern xmlrpc_value* xmlrpc_del_repo(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo);

#endif

