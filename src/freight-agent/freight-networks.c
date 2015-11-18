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
#include <freight-networks-private.h>

struct network *active_networks = NULL;

static unsigned int bridgenum = 0;
#if 0
static unsigned int physnum = 0;

#endif
static unsigned int containernum = 0;
static unsigned int connectornum = 0;

static char* generate_bridgename()
{
	char idx[11];
	snprintf(idx, 11, "%u", bridgenum++);
	return strjoin("frbr", idx, NULL);
}
#if 0
static char* generate_physname()
{
	char idx[11];
	snprintf(idx, 11, "%u", physnum++);
	return strjoin("frpy", idx, NULL);
}
#endif

static char* generate_containername()
{
	char idx[11];
	snprintf(idx, 11, "%u", containernum++);
	return strjoin("frc", idx, NULL);
}

static char *generate_connectorname()
{
	char idx[11];
	snprintf(idx, 11, "%u", connectornum++);
	return strjoin("frb2b", idx, NULL);
}

static int interface_exists(char *ifc)
{
	char *cmd = strjoina("ip link show ", ifc, NULL);

	return !run_command(cmd, 0);
}

static void delete_interface(const char *ifc)
{
	char *cmd = strjoina("ip link del dev ", ifc, NULL);

	run_command(cmd, 0);
}

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

static struct network* get_network_entry(const char *network, const char *tennant)
{
	struct network *idx = active_networks;

	while (idx != NULL) {
		if (!strcmp(network, idx->network) && 
		    !strcmp(tennant, idx->tennant))
			return idx;
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
	new->bridge = generate_bridgename();
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

static void hdetach_pbridge_from_bridge(struct network *net, const struct agent_config *acfg)
{
	char *ifc = strjoina(net->physifc, "a");
	delete_interface(ifc);
}

static int hattach_pbridge_to_bridge(struct network *net, const struct agent_config *acfg)
{
	char *cmd;
	int rc = 0;
	char *neta, *netb;
	char *ifc = generate_connectorname();


	net->physifc = ifc;
	neta = strjoina(ifc, "a");
	netb = strjoina(ifc, "b");

	cmd = strjoin("ip link add dev ", neta, " type veth ",
                              "peer name ", netb, NULL);
	rc = run_command(cmd, 0);
	free(cmd);
	if (rc) {
		LOG(ERROR, "Unable to create %s veth pair\n", ifc);
		goto out;
	}

	cmd = strjoin("ip link set dev ", neta, " master ", acfg->node.host_bridge, NULL);
	rc = run_command(cmd, 0);
	free(cmd);
	if (rc) {
		LOG(ERROR, "Unable to attach %s to public bridge\n", ifc);
		goto out_destroy;
	}

	cmd = strjoin("ip link set dev ", netb, " master ", net->bridge, NULL);
	rc = run_command(cmd, 0);
	free(cmd);
	if (rc) {
		LOG(ERROR, "Unable to attach %s to tennant bridge %s\n", ifc, net->bridge);
		goto out_destroy;
	}

out:
	return rc;
out_destroy:
	delete_interface(neta);
	goto out;
	
}


struct host_net_methods {
	int (*host_attach)(struct network *net, const struct agent_config *acfg);
	void (*host_detach)(struct network *net, const struct agent_config *acfg);
};

static struct host_net_methods host_attach_methods[] = {
	[NET_TYPE_BRIDGED] = {hattach_pbridge_to_bridge, hdetach_pbridge_from_bridge},
	{NULL, NULL}
};

static int attach_host_network_to_bridge(struct network *net, const struct agent_config *acfg)
{
	return host_attach_methods[net->conf->type].host_attach(net, acfg);
}

static void detach_host_network_from_bridge(struct network *net, const struct agent_config *acfg)
{
	host_attach_methods[net->conf->type].host_detach(net, acfg);
}

static struct network* add_network_bridge(const char *network, const char *tennant,
					  const char *cfstring,  const struct agent_config *acfg)
{
	struct network *new, *old;
	int rc = 0;

	old = get_network_entry(network, tennant);

	if (old)
		new = old;
	else
		new = alloc_network_entry(network, tennant);

	if (!new)
		goto out;

	if (!old) {
		rc = parse_network_configuration(cfstring, new, acfg);
		if (rc)
			goto out_free;
	}

	if (!interface_exists(new->bridge))
		rc = create_bridge_from_entry(new, acfg);
	else
		LOG(INFO, "Bridge %s already exists\n", new->bridge);

	if (rc)
		goto out_free;

	if (!old)
		link_network_bridge(new);	

out:
	return new;

out_free:
	free_network_entry(new);
	new = NULL;
	goto out;
}

static void remove_network_bridge(struct network *net, const struct agent_config *acfg)
{
	/*
	 * Unlink it from the list
	 */
	unlink_network_bridge(net);

	/*
	 * Then remove the host interface from the bridge
	 */
	detach_host_network_from_bridge(net, acfg);

	/*
	 * Then remove the bridge itself
	 */
	delete_interface(net->bridge);

	/*
	 * And free the net entry
	 */
	free_network_entry(net);
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
		if (get_network_entry(netname, tennant)) {
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
	remove_network_bridge(new, acfg);
	goto out_free;
}

extern void cleanup_networks_on_host(const char *container, const char *tennant,
				     const struct agent_config *acfg)
{
	struct network *net;
	const struct ifc_list *list;

	/*
	 * First build an interface list for the container and remove those
	 */
	list = build_interface_list_for_container(container, tennant, acfg);
	detach_and_destroy_container_interfaces(list, acfg);

	for (net = active_networks; net; net = net->next) {
		if (!net->clients) {
			LOG(INFO, "REMOVING BRIDGE %s\n", net->bridge);
			remove_network_bridge(net, acfg);
		}
	}
}


const struct ifc_list* build_interface_list_for_container(const char *container, const char *tennant, const struct agent_config *acfg)
{
	struct tbl *networks;
	char *filter;
	size_t size;
	struct network *net;
	const char *netname;
	char *bridge_veth, *container_veth;
	char *basename;
	int i;

	struct ifc_list *list = NULL;

	filter = strjoina("tennant='", tennant, "' AND name='",
			  container,"'",NULL);

	networks = get_raw_table(TABLE_NETMAP, filter, acfg);


	if (!networks->rows)
		goto out_free;

	size = sizeof(struct ifc_list) + (sizeof(struct ifc_info)*networks->rows);
	list =  calloc(1, size);
	list->count = networks->rows;

	for (i=0; i < networks->rows; i++) {
		netname = lookup_tbl(networks, i, COL_CNAME);
		net = get_network_entry(netname, tennant);
		if (!net) {
			LOG(ERROR, "net %s hasn't been established!\n", netname);
			continue;
		}
		basename = generate_containername();
		bridge_veth = strjoin(basename, "b", NULL);
		container_veth = strjoin(basename,"c", NULL);
		free(basename);

		list->ifc[i].container_veth = container_veth;
		list->ifc[i].bridge_veth = bridge_veth;
		list->ifc[i].ifcdata = net;
		net->clients++;

	}
out_free:
	free_tbl(networks);
	return list;	
}

int detach_and_destroy_container_interfaces(const struct ifc_list *list, const struct agent_config *acfg)
{
	int i;
	struct network *net;

	for (i = 0; i < list->count; i++) {
		net = list->ifc[i].ifcdata;
		delete_interface(list->ifc[i].container_veth);
		net->clients--;
	}

	return 0;
}


int create_and_bridge_interface_list(const struct ifc_list *list, const struct agent_config *acfg)
{
	int i, rc;
	char *cmd;
	struct network *net;

	for (i=0; i < list->count; i++) {

		LOG(DEBUG, "addressing entry %d/%d\n", i, (int)list->count);

		net = list->ifc[i].ifcdata;

		/*
		 * Allocate the device pair
		 */
		LOG(DEBUG, "Creating veth pair\n");
		cmd = strjoin("ip link add dev ", list->ifc[i].container_veth, " type veth ",
			      "peer name ", list->ifc[i].bridge_veth, NULL);

		rc = run_command(cmd, 1);

		free(cmd);
		if (rc) {
			LOG(ERROR, "Could not create veth pair %s:%s\n", list->ifc[i].container_veth,
				list->ifc[i].bridge_veth);
			continue;
		}

		((struct ifc_list *)list)->ifc[i].state = IFC_CREATED;

		/*
		 * Then attach the bridge veth to the bridge
		 */
		cmd = strjoin("ip link set dev ", list->ifc[i].bridge_veth, " master ", net->bridge, NULL);
		LOG(DEBUG, "Attaching to bridge\n");
		rc = run_command(cmd, 0);
		free(cmd);
		if (rc) {
			LOG(ERROR, "Could not attach %s to bridge %s\n", list->ifc[i].bridge_veth, net->bridge);
			delete_interface(list->ifc[i].bridge_veth);
			continue;
		}
		net->clients++;
		((struct ifc_list *)list)->ifc[i].state = IFC_ATTACHED;
	}

	LOG(DEBUG, "Done\n");
	return 0;
}


void free_interface_list(const struct ifc_list *list)
{
	int i;
	for (i=0; i < list->count; i++) {
		free((void *)list->ifc[i].container_veth);
		free((void *)list->ifc[i].bridge_veth);
	}
	free((void *)list);
}


