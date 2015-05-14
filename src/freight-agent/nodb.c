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
 * *File: nodb.c
 * *
 * *Author:Neil Horman
 * *
 * *Date:
 * *
 * *Description implements access to an NULL db
 * *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <libpq-fe.h>
#include <string.h>
#include <freight-log.h>
#include <freight-db.h>

struct postgres_info {
	PGconn *conn;
};

static int nodb_init(struct agent_config *acfg)
{
	acfg->db.db_priv = NULL;
	return 0;
}

static void nodb_cleanup(struct agent_config *acfg)
{
	return;
}

static int nodb_disconnect(struct agent_config *acfg)
{
	return 0;
}

static int nodb_connect(struct agent_config *acfg)
{
	return 0;
}


static struct yum_cfg_list* nodb_get_yum_cfg(const struct agent_config *acfg)
{
	struct yum_cfg_list *repos = NULL;

	repos = calloc(1, sizeof(struct yum_cfg_list));
	if (!repos) {
		LOG(INFO, "No memory to report yum configuration\n");
		goto out;
	}
out:
	return repos;
}

static void nodb_free_yum_cfg(struct yum_cfg_list *repos)
{
	free(repos);
}

struct tbl* nodb_get_table(const char *tbl, const char *cols, const char *filter,
                                 const struct agent_config *acfg)
{
	struct tbl *table = NULL;

	/*
 	 * For this local dummy database, we always just return a local tennant 
 	 * From the tennant_hosts table
 	 */
	if (!strcmp(tbl, "tennant_hosts")) {
		table = alloc_tbl(1,1);
		if (table)
			table->value[0][0] = strdup("local");
	}

	return table;
}

struct db_api nodb_api = {
	.init = nodb_init,
	.cleanup = nodb_cleanup,
	.connect = nodb_connect,
	.disconnect = nodb_disconnect,
	.get_yum_cfg = nodb_get_yum_cfg,
	.free_yum_cfg = nodb_free_yum_cfg,
	.get_table = nodb_get_table,
};
