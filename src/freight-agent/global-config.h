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
 * *File: global-config.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description: Configur structures for freight-agent 
 * *********************************************************/


#ifndef _GLOBAL_CONFIG_H_
#define _GLOBAL_CONFIG_H_

#include <freight-config.h>

struct global_cfg {
	int db_version;
	int base_interval;
	int gc_multiple;
	int healthcheck_multiple;
};


extern int refresh_global_config(struct global_cfg *cfg, const struct agent_config *acfg);



#endif
