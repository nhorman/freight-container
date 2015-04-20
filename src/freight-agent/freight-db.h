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
 * *File: freight-db.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description API for DB connections
 * *********************************************************/


#ifndef _FREIGHT_DB_H_
#define _FREIGHT_DB_H_
#include <stdio.h>
#include <errno.h>
#include <freight-config.h>


struct db_api {
	int (*init)(struct agent_config *acfg);
	void (*cleanup)(struct agent_config *acfg);
	int (*connect)(struct agent_config *acfg);
	int (*disconnect)(struct agent_config *acfg);
};

extern db_api postgres_db_api;

static inline struct *db_api get_db_api(struct agent_config *acfg) {

	switch (acfg->db.dbtype) {
	case DB_TYPE_POSTGRES:
		return &postgres_db_api;
	default:
		return NULL;
	}
}

static inline int db_init(struct db_api *api, struct agent_config *acfg)
{
	if (!db_api->init)
		return -EOPNOTSUPP;
	return db_api->init(acfg);	
}

static inline void db_cleanup(struct db_api *api, struct agent_config *acfg)
{
	if (!db_api->cleanup)
		return -EOPNOTSUPP;
	return db_api->cleanup(acfg);	
}

static inline int db_connect(struct db_api *api, struct agent_config *acfg)
{
	if (!db_api->connect)
		return -EOPNOTSUPP;
	return db_api->connect(acfg);	
}

static inline int db_disconnect(struct db_api *api, struct agent_config *acfg)
{
	if (!db_api->init)
		return -EOPNOTSUPP;
	return db_api->disconnect(acfg);	
}

#endif
