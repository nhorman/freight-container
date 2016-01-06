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

#define XMLRPC_HANDLER(op) extern xmlrpc_value* (op)(xmlrpc_env * const envp, xmlrpc_value * const params, void * serverinfo, void *callinfo)

XMLRPC_HANDLER(get_table);
XMLRPC_HANDLER(xmlrpc_add_repo);
XMLRPC_HANDLER(xmlrpc_del_repo);
XMLRPC_HANDLER(xmlrpc_create_container);
XMLRPC_HANDLER(xmlrpc_delete_container);
XMLRPC_HANDLER(xmlrpc_boot_container);
XMLRPC_HANDLER(xmlrpc_poweroff_container);
XMLRPC_HANDLER(xmlrpc_create_network);
XMLRPC_HANDLER(xmlrpc_delete_network);
XMLRPC_HANDLER(xmlrpc_attach_network);
XMLRPC_HANDLER(xmlrpc_detach_network);
XMLRPC_HANDLER(xmlrpc_update_config);

#endif

