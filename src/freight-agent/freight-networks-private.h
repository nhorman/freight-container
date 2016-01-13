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
	AQUIRE_EXTERNAL_STATIC,
};

struct static_config {
	char *addr_start;
	char *addr_end;
	char *netmask;
	char *dns;
	char *defroute;
};

struct address_config {
	enum aquire_type ipv4;
	struct static_config ipv4_config;
	enum aquire_type ipv6;
	struct static_config ipv6_config;
};

struct netconf {
	enum network_type type;
	struct address_config aconf;
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
