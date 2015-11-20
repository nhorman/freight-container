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
 * *File: freight-networks.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description API for freight container network setup 
 * *********************************************************/


#ifndef _FREIGHT_NETWORKS_H_
#define _FREIGHT_NETWORKS_H_
#include <stdio.h>
#include <errno.h>
#include <freight-db.h>

/*
 * Given a container and tennant, find all the networks this container is attached to
 * and create a bridge instance for it
 */
extern int establish_networks_on_host(const char *container, const char *tennant,
				      const struct agent_config *acfg);

extern void cleanup_networks_on_host(const char *container, const char *tennatn,
				     const struct agent_config *acfg);

enum ifc_state {
	IFC_NONE = 0,
	IFC_CREATED,
	IFC_ATTACHED
};

struct ifc_info {
	const char *container_veth;
	const char *bridge_veth;
	void *ifcdata;
	enum ifc_state state;
};

struct ifc_list {
	size_t count;
	struct ifc_info ifc[0];
};

extern const struct ifc_list* build_interface_list_for_container(const char *container, const char *tennant, const struct agent_config *acfg);
extern int create_and_bridge_interface_list(const struct ifc_list *list, const struct agent_config *acfg);
extern int detach_and_destroy_container_interfaces(const struct ifc_list *list, const struct agent_config *acfg);
extern void free_interface_list(const struct ifc_list *list);


extern int get_address_for_interfaces(struct ifc_list *list, const char *container, const struct agent_config *acfg);
extern void release_address_for_interfaces(struct ifc_list *list, const struct agent_config *acfg);

extern int setup_networks_in_container(const char *cname, const char *iname, const char *tennant, const struct ifc_list *list, const struct agent_config *acfg);

#endif
