/*********************************************************
 * *Copyright (C) 2015 Neil Horman
 * *This program is free software; you can redistribute it and\or modify
 * *it under the terms of the GNU General Public License as published 
 * *by the Free Software Foundation; either version 2 of the License,
 * *or  any later version.
 * *
 * *This program is distributed in the hope that it will be useful,
 * *but WITHOUT ANY WARRANTY; without even the implied warranty of
 * *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * *GNU General Public License for more details.
 * *
 * *File: mode.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description: 
 * *********************************************************/


#ifndef _MODE_H_
#define _MODE_H_
#include <freight-config.h>
#include <freight-db.h>
#include <freight-networks.h>

int init_container_root(const struct agent_config *acfg);

void clean_container_root(const struct agent_config *acfg);

int install_container(const char *rpm, const char *tennant,
		      const struct agent_config *acfg);

int install_and_update_container(const char *rpm, const char *tennant,
			      const struct agent_config *acfg);

int uninstall_container(const char *rpm, const char *tennant,
			struct agent_config *acfg);

int enter_mode_loop(struct agent_config *config);

void list_containers(char *scope, const char *tennant,
		     struct agent_config *config);

int exec_container(const char *rpm, const char *name,
		   const char *tennant, const struct ifc_list *ifcs,
		   int should_fork, const struct agent_config *acfg);

int poweroff_container(const char *iname, const char *cname, const char *tennant,
		   const struct agent_config *acfg);

/*
 * These are convienience definitions that allow for local operation
 * where we imply the 'local' tennant for local use
 */
#define local_install_container(r, c) install_container(r, "local", c)
#define local_uninstall_container(r, c) uninstall_container(r, "local", c)
#define local_list_containers(s, c) list_containers(s, "local", c)
#define local_exec_container(r, n, c) exec_container(r, n, "local", NULL, 0, c)

#endif
