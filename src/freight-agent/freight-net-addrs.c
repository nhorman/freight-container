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
 * *Description API for freight container network address management 
 * *********************************************************/


#include <stdio.h>
#include <errno.h>
#include <freight-networks.h>
#include <freight-common.h>
#include <libconfig.h>
#include <freight-networks-private.h>

enum addr_type {
	IPV4 = 0,
	IPV6 = 1
};

static int allocate_address_from_db(const char *ifc, struct ifc_info *info, enum addr_type type, const struct agent_config *acfg)
{
	struct network *net = info->ifcdata;
	char *address;

	if (type == IPV4) {
		address = alloc_db_v4addr(net->network, net->tennant,
					  net->conf->aconf.ipv4_config.addr_start,
					  net->conf->aconf.ipv4_config.addr_end, acfg);
		if (address){
			strncpy(info->esd.container_v4addr, address, 16);
			free(address);
		} else
			return -ENOENT;
	} else { 
		address = alloc_db_v6addr(net->network, net->tennant,
					  net->conf->aconf.ipv6_config.addr_start,
					  net->conf->aconf.ipv6_config.addr_end, acfg);
		if (address) {
			strncpy(info->esd.container_v6addr, address, 256);
			free(address);
		} else
			return -ENOENT;
	}

	return 0;
}

static void release_address_to_db(const char *ifc, struct ifc_info *info, enum addr_type type, const struct agent_config *acfg)
{
	struct network *net = info->ifcdata;
	if (type == IPV4)
		release_db_v4addr(net->network, net->tennant, info->esd.container_v4addr, acfg);
	else
		release_db_v6addr(net->network, net->tennant, info->esd.container_v6addr, acfg);
}

struct addr_management {
	int (*acquire)(const char *ifc, struct ifc_info *info, enum addr_type type, const struct agent_config *acfg);
	void (*release)(const char *ifc, struct ifc_info *info, enum addr_type type, const struct agent_config *acfg);
};

struct addr_management mgmt_type[] = {
	[AQUIRE_NONE] = {NULL, NULL},
	[AQUIRE_DHCP] = {NULL, NULL },
        [AQUIRE_DHCPV6] = {NULL, NULL},
        [AQUIRE_SLAAC] = {NULL, NULL},
        [AQUIRE_EXTERNAL_STATIC] = {allocate_address_from_db, release_address_to_db},
};

static int acquire_address_for_ifc(const char *ifc, struct ifc_info *info, const struct agent_config *acfg)
{
	struct network *net = info->ifcdata;
	int rc = 0;

	if (mgmt_type[net->conf->aconf.ipv4].acquire)
		rc = mgmt_type[net->conf->aconf.ipv4].acquire(ifc, info, IPV4, acfg);

	if (rc)
		return rc;

	if (mgmt_type[net->conf->aconf.ipv6].acquire) {
		rc = mgmt_type[net->conf->aconf.ipv6].acquire(ifc, info, IPV6, acfg);
		if (rc && mgmt_type[net->conf->aconf.ipv4].release)
			mgmt_type[net->conf->aconf.ipv4].release(ifc, info, IPV4, acfg);
	}

	return rc;

}

int get_address_for_interfaces(struct ifc_list *list, const char *container, const struct agent_config *acfg)
{
	int i;
	int rc;

	rc = 0;
	/*
	 * We have to do this for every interface
	 */
	for (i = 0; i < list->count; i ++) {

		/*
		 * If we make it here the we need to use the default policy for the network
		 */
		rc = acquire_address_for_ifc(list->ifc[i].container_veth, &list->ifc[i], acfg); 
		if (rc)
			LOG(WARNING, "Failed to get address for ifc %s on container %s\n", 
				list->ifc[i].container_veth, container);

	} 

	return rc;
}

void release_address_for_interfaces(struct ifc_list *list, const struct agent_config *acfg)
{

}


