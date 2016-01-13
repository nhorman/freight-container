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


struct addr_management {
	int (*acquire)(const char *ifc, const struct network *net, const struct agent_config *acfg);
	void (*release)(const char *ifc, const struct network *net, const struct agent_config *acfg);
};


struct addr_management mgmt_type[] = {
	[AQUIRE_NONE] = {NULL, NULL},
	[AQUIRE_DHCP] = {NULL, NULL },
        [AQUIRE_DHCPV6] = {NULL, NULL},
        [AQUIRE_SLAAC] = {NULL, NULL},
        [AQUIRE_STATIC] = {NULL, NULL},
};

#if 0
static void release_address_for_ifc(const char *ifc, const struct network *net, const struct agent_config *acfg)
{
	if (mgmt_type[net->conf->aconf.ipv4].release)
		mgmt_type[net->conf->aconf.ipv4].release(ifc, net, acfg);

	if (mgmt_type[net->conf->aconf.ipv6].release)
		mgmt_type[net->conf->aconf.ipv6].release(ifc, net, acfg);
}
#endif

static int acquire_address_for_ifc(const char *ifc, const struct network *net, const struct agent_config *acfg)
{
	int rc = 0;

	if (mgmt_type[net->conf->aconf.ipv4].acquire)
		rc = mgmt_type[net->conf->aconf.ipv4].acquire(ifc, net, acfg);

	if (rc)
		return rc;

	if (mgmt_type[net->conf->aconf.ipv6].acquire) {
		rc = mgmt_type[net->conf->aconf.ipv6].acquire(ifc, net, acfg);
		if (rc && mgmt_type[net->conf->aconf.ipv4].release)
			mgmt_type[net->conf->aconf.ipv4].release(ifc, net, acfg);
	}

	return rc;

}
#if 0
static int assign_static_v4_address(const char *ifc, const char *addr, const struct agent_config *acfg)
{
	char *cmd;

	cmd = strjoina("ip addr add ", addr, " dev ", ifc, NULL);

	return run_command(cmd, 0);
}
#endif

int get_address_for_interfaces(struct ifc_list *list, const char *container, const struct agent_config *acfg)
{
	struct network *net;
	int i;
	int rc;

	rc = 0;
	/*
	 * We have to do this for every interface
	 */
	for (i = 0; i < list->count; i ++) {
		net = (struct network *)list->ifc[i].ifcdata;

		/*
		 * If we make it here the we need to use the default policy for the network
		 */
		rc = acquire_address_for_ifc(list->ifc[i].container_veth, net, acfg); 
		if (rc)
			LOG(WARNING, "Failed to get address for ifc %s on container %s\n", 
				list->ifc[i].container_veth, container);

	} 

	return rc;
}

void release_address_for_interfaces(struct ifc_list *list, const struct agent_config *acfg)
{

}


