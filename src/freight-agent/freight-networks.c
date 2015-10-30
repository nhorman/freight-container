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


#include <stdio.h>
#include <errno.h>
#include <freight-networks.h>
#include <freight-common.h>

struct network {
	char *tennant;
	char *network;
	char *bridge;
	struct network *next;
};

struct network *active_networks = NULL;

static const char* get_network_bridge(const char *network, const char *tennant)
{
	struct network *idx = active_networks;

	while (idx != NULL) {
		if (!strcmp(network, idx->network) && 
		    !strcmp(tennant, idx->tennant))
			return idx->bridge;
	}

	return NULL;
}

static struct network* alloc_network_entry(const char *network, const char *tennant)
{
	struct network *new;

	new = calloc(1, sizeof(struct network));

	if (!new)
		return NULL;

	/*
	 * Fill out the fields in the network struct
	 */
	
	new->tennant = strdup(tennant);
	new->network = strdup(network);
	new->bridge = calloc(1,strlen(tennant)+strlen(network)+2);
	sprintf(new->bridge, "%s-%s", tennant, network);
	return new;
	
}

static void free_network_entry(struct network *ptr)
{
	free(ptr->tennant);
	free(ptr->network);
	free(ptr->bridge);
	free(ptr);
}

static void link_network_bridge(struct network *ptr)
{
	ptr->next = active_networks;
	active_networks = ptr;
}

#if 0 /* Unused for now */
static void unlink_network_bridge(struct network *ptr)
{
	struct network *idx = active_networks;
	struct network *tmp = active_networks;

	while (idx) {
		if (idx == ptr) {
			idx->next = ptr->next;
			ptr->next = NULL;
		}
		idx = idx->next;	
		if (idx && (tmp->next != idx))
			tmp = tmp->next;
	}
}
#endif

static int create_bridge_from_entry(struct network *net, const struct agent_config *acfg)
{
	char *cmd;
	int rc;

	cmd = strjoina("ip link add name ", net->bridge, " type bridge", NULL);

	rc = run_command(cmd, 0);	

	if (rc) {
		LOG(ERROR, "Unable to create bridge %s\n", net->bridge);
		goto out;
	}

out:
	return rc;
}

static void remove_network_bridge(struct network *ptr)
{

	char *cmd;
	int rc;

	cmd = strjoina("ip link delete dev ", ptr->bridge);

	rc = run_command(cmd, 0);

	if (rc) {
		LOG(ERROR, "Unable to delete bridge %s\n", ptr->bridge);
	}

	return;
}

static struct network* add_network_bridge(const char *network, const char *tennant, const struct agent_config *acfg)
{
	struct network *new;
	int rc;

	new = alloc_network_entry(network, tennant);

	if (!new)
		goto out;

	rc = create_bridge_from_entry(new, acfg);

	if (rc)
		goto out_free;

out:
	return new;
out_free:
	free_network_entry(new);
	new = NULL;
	goto out;
}

static int attach_network_to_bridge(struct network *net, const char *cfstring, const struct agent_config *acfg)
{
	return 0;
}


int establish_networks_on_host(const char *container, const char *tennant,
			       const struct agent_config *acfg)
{

	struct tbl *networks;
	char *filter;
	int rc = 0;
	int i;
	struct tbl *netinfo;
	char *netname;
	char *cfstring;
	struct network *new;

	filter = strjoina("tennant='",tennant,"' AND name='",container,"'",NULL);

	networks = get_raw_table(TABLE_NETMAP, filter, acfg);

	if (!networks->rows)
		goto out;

	/*
	 * For each network for this container, check out list to see if
	 * we have created a corresponding bridge.  If we haven't, create it
	 */
	for(i=0; i < networks->rows; i++) {
		netname = lookup_tbl(networks, i, COL_CNAME);

		netinfo = get_network_info(netname, tennant,  acfg);
		if (!netinfo)
			continue;

		cfstring = lookup_tbl(netinfo, 0, COL_CONFIG);

		/*
		 * if we already have the network, just go on
		 */
		if (get_network_bridge(netname, tennant)) {
			free_tbl(netinfo);
			continue;
		}

		new = add_network_bridge(netname, tennant, acfg);
		if (new) {
			LOG(ERROR, "Failed to add network bridge for network %s:%s\n",
				netname, tennant);
			rc = EINVAL;
			free_tbl(netinfo);
			goto out_free;
		}

		rc = attach_network_to_bridge(new, cfstring, acfg);
		if (rc) {
			LOG(ERROR, "Failed to attach physical network to bridge %s\n", new->bridge);
			free_tbl(netinfo);
			goto out_remove_bridge;
		}

		free_tbl(netinfo);
		link_network_bridge(new);

	}

out_free:
	free_tbl(networks);
out:	
	return rc;
out_remove_bridge:
	remove_network_bridge(new);
	free_network_entry(new);
	goto out_free;
}


const char *get_bridge_for_container(const char *network, const char *tennant,
				     const struct agent_config *acfg)
{
	return NULL;
}

