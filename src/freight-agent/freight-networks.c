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
#include <libconfig.h>

enum network_type {
	NET_TYPE_BRIDGED = 0,
};

enum aquire_type {
	AQUIRE_NONE = 0,
	AQUIRE_DHCP,
	AQUIRE_DHCPV6,
	AQUIRE_SLAAC,
	AQUIRE_STATIC,
};

struct static_entry {
	char *cname;
	char *ipv4_address;
	char *ipv6_address;
};

struct address_config {
	enum aquire_type ipv4;
	enum aquire_type ipv6;
};
	
struct netconf {
	enum network_type type;
	struct address_config aconf;
	/* network type config should go here */
	unsigned int static_entries;
	struct static_entry entries[0];
};

struct network {
	char *tennant;
	char *network;
	char *bridge;
	struct netconf *conf;
	int clients;
	struct network *next;
};

struct network *active_networks = NULL;

static int parse_network_type(config_t *config, struct netconf *conf)
{
	config_setting_t *network = config_lookup(config, "network");
	config_setting_t *type;
	const char *typestr;
	int rc = -ENOENT;

	if (!network)
		return -ENOENT;

	type = config_setting_get_member(network, "type");
	if (!type)
		return -ENOENT;

	typestr = config_setting_get_string(type);	

	if (!strcmp(typestr, "bridged")) {
		rc = 0;
		conf->type = NET_TYPE_BRIDGED;
	}

	return rc;
}

static int parse_address_config(config_t *config, struct netconf *conf)
{
	config_setting_t *acfg = config_lookup(config, "address_config");
	config_setting_t *type;
	const char *typestr;
	int rc = 0;

	if (!acfg)
		return -ENOENT;

	type = config_setting_get_member(acfg, "ipv4_aquisition");
	if (!type)
		return -ENOENT;
	typestr = config_setting_get_string(type);	
	if (!strcmp(typestr, "dhcp"))
		conf->aconf.ipv4 = AQUIRE_DHCP;
	else if (!strcmp(typestr, "static"))
		conf->aconf.ipv4 = AQUIRE_STATIC;
	else {
		rc = -EINVAL;
		goto out;
	}

	type = config_setting_get_member(acfg, "ipv6_aquisition");
	if (type) {
		typestr = config_setting_get_string(type);	
		if (!strcmp(typestr, "dhcpv6"))
			conf->aconf.ipv4 = AQUIRE_DHCPV6;
		else if (!strcmp(typestr, "static"))
			conf->aconf.ipv4 = AQUIRE_STATIC;
		else if (!strcmp(typestr, "slaac"))
			conf->aconf.ipv4 = AQUIRE_SLAAC;
		else {
			rc = -EINVAL;
			goto out;
		}
	}

out:
	return rc;
}


static int parse_network_configuration(const char *cfstring, struct network *net,
				       const struct agent_config *acfg)
{
	config_t config;
	config_setting_t *tmp;
	int rc;
	int entry_cnt = 0;

	config_init(&config);

	rc = config_read_string(&config, cfstring);
	if (rc == CONFIG_FALSE) {
		LOG(ERROR, "Could not read network configuration on line %d: %s\n",
			config_error_line(&config), config_error_text(&config));
		rc = -EINVAL;
		goto out;
	}

	/*
	 * We have to count the number of static entries before we can allocate
	 * the config struct
	 */
	tmp = config_lookup(&config, "static_address");

	if (tmp)
		entry_cnt = config_setting_length(tmp);

	net->conf = calloc(1,sizeof(struct netconf)+((sizeof(struct static_entry)*entry_cnt)));

	if (!net->conf) {
		LOG(ERROR, "Unable to allocate memory for network config\n");
		rc = -EINVAL;
		goto out_destroy;
	}

	net->conf->static_entries = entry_cnt;

	rc = parse_network_type(&config, net->conf);
	if (rc) {
		LOG(ERROR, "Error determining network type: %s\n", strerror(rc));
		goto out_destroy;
	}

	rc = parse_address_config(&config, net->conf);
	if (rc) {
		LOG(ERROR, "Error parsing address config\n");
		goto out_destroy;
	}

out_destroy:
	config_destroy(&config);
out:
	return rc;
}

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
	free(ptr->conf);
	free(ptr);
}

static void link_network_bridge(struct network *ptr)
{
	ptr->next = active_networks;
	active_networks = ptr;
}

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

	unlink_network_bridge(ptr);
	free_network_entry(ptr);
	return;
}

static struct network* add_network_bridge(const char *network, const char *tennant,
					  const char *cfstring,  const struct agent_config *acfg)
{
	struct network *new;
	int rc;

	new = alloc_network_entry(network, tennant);

	if (!new)
		goto out;

	rc = parse_network_configuration(cfstring, new, acfg);
	if (rc)
		goto out_free;

	rc = create_bridge_from_entry(new, acfg);

	if (rc)
		goto out_free;

	link_network_bridge(new);	

out:
	return new;
out_free:
	free_network_entry(new);
	new = NULL;
	goto out;
}

static int hattach_macvlan_to_bridge(struct network *net, const struct agent_config *acfg)
{
	char *cmd;
	int rc = 0;

	/* Create the macvlan interface */
	cmd = strjoin("ip link add link ", acfg->node.host_ifc, " name mvl-",
		      net->tennant,"-",net->network, " type macvlan", NULL);

	rc = run_command(cmd, 0);
	if (rc) {
		LOG(ERROR, "Unable to create a macvlan interface mvl-%s-%s : %s\n",
			net->tennant, net->network, strerror(rc));
		goto out;
	}

	/* Set the interface into promisc mode */
	free(cmd);
	cmd = strjoin("ip link set dev mvl-", net->tennant, "-", net->network," promisc on", NULL);
	rc = run_command(cmd, 0);
	if (rc) {
		LOG(ERROR, "Unable to set macvlan mvl-%s-%s into promisc mode\n",
			net->tennant, net->network);
		goto out_destroy;
	}

	/* And attach it to the bridge */
	free(cmd);
	cmd = strjoin("ip link set dev mvl-", net->tennant, "-", net->network, 
		      " master ", net->bridge, NULL);
	rc = run_command(cmd, 0);
	if (rc) {
		LOG(ERROR, "Unable to attach mvl-%s-%s to bridge %s\n",
			net->tennant, net->network, net->bridge);
		goto out_destroy;
	}
	free(cmd);
out:
	return rc;
out_destroy:
	free(cmd);
	cmd = strjoin("ip link del dev mvl-", net->tennant, "-", net->network, NULL);
	run_command(cmd, 0);
	goto out;
}

typedef int (*host_attach)(struct network *net, const struct agent_config *acfg);

host_attach host_attach_methods[] = {
	[NET_TYPE_BRIDGED] = hattach_macvlan_to_bridge,
	NULL,
};

static int attach_host_network_to_bridge(struct network *net, const struct agent_config *acfg)
{
	return host_attach_methods[net->conf->type](net, acfg);
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

		new = add_network_bridge(netname, tennant, cfstring, acfg);
		if (!new) {
			LOG(ERROR, "Failed to add network bridge for network %s:%s\n",
				netname, tennant);
			rc = EINVAL;
			free_tbl(netinfo);
			goto out_free;
		}

		rc = attach_host_network_to_bridge(new, acfg);
		if (rc) {
			LOG(ERROR, "Failed to attach physical network to bridge %s\n", new->bridge);
			free_tbl(netinfo);
			goto out_remove_bridge;
		}
		free_tbl(netinfo);

	}

out_free:
	free_tbl(networks);
out:	
	return rc;
out_remove_bridge:
	remove_network_bridge(new);
	goto out_free;
}


const char *get_bridge_for_container(const char *network, const char *tennant,
				     const struct agent_config *acfg)
{
	return NULL;
}

