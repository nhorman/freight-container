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


#ifndef _FREIGHT_NETWORKS_PRIVATE_H_
#define _FREIGHT_NETWORKS_PRIVATE_H_



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
	char *physifc;
	struct netconf *conf;
	int clients;
	struct network *next;
};

#endif
