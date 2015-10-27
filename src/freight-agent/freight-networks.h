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

/*
 * Look up the local bridge instance name of a network for a given tennatn
 */
extern const char *get_bridge_for_container(const char *network, const char *tennant,
					    const struct agent_config *acfg);

#endif
