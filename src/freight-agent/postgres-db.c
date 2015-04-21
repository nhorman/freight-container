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
 * *File: postgres-db.c
 * *
 * *Author:Neil Horman
 * *
 * *Date:
 * *
 * *Description implements access to postgres db 
 * *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <libpq-fe.h>
#include <freight-log.h>
#include <freight-db.h>

struct postgres_info {
	PGconn *conn;
};

static int pg_init(struct agent_config *acfg)
{
	struct postgres_info *info = calloc(1, sizeof(struct postgres_info));
	if (!info)
		return -ENOMEM;
	acfg->db.db_priv = info;
	return 0;
}

static void pg_cleanup(struct agent_config *acfg)
{
	struct postgres_info *info = acfg->db.db_priv;

	free(info);
	acfg->db.db_priv = NULL;
	return;
}

static int pg_disconnect(struct agent_config *acfg)
{
	struct postgres_info *info = acfg->db.db_priv;
	PQfinish(info->conn);
	return 0;
}

static int pg_connect(struct agent_config *acfg)
{
	int rc = -ENOTCONN;

	struct postgres_info *info = acfg->db.db_priv;

	char * k[] = {
		"hostaddr",
		"dbname",
		"user",
		"password",
		NULL
	};
	const char *const * keywords = (const char * const *)k;

	char *v[] = {
		acfg->db.hostaddr,
		acfg->db.dbname,
		acfg->db.user,
		acfg->db.password,
		NULL
	};

	const char *const * values = (const char * const *)v;

	info->conn = PQconnectdbParams(keywords, values, 0);

	if (PQstatus(info->conn) == CONNECTION_OK)
		LOG(INFO, "freight-agent connection...Established!\n");
	else {
		LOG(INFO, "freight-agent connection...Failed: %s\n",
			PQerrorMessage(info->conn));
		pg_disconnect(acfg);
		goto out;
	}	
	rc = 0;

out:
	return rc;
}

struct db_api postgres_db_api = {
	.init = pg_init,
	.cleanup = pg_cleanup,
	.connect = pg_connect,
	.disconnect = pg_disconnect,
};
