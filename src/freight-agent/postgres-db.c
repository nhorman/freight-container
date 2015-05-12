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
#include <string.h>
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


static struct yum_cfg_list* pg_get_yum_cfg(const struct agent_config *acfg)
{
	struct postgres_info *info = acfg->db.db_priv;
	PGresult *result;
	ExecStatusType rc;
	int yum_entries;
	size_t alloc_size;
	struct yum_cfg_list *repos = NULL;

	result = PQexec(info->conn, "SELECT * FROM yum_config");

	rc = PQresultStatus(result);
	if (rc != PGRES_TUPLES_OK) {
		LOG(ERROR, "Unable to query yum_config table: %s\n",
			PQresultErrorMessage(result));
		goto out;
	}

	yum_entries = PQntuples(result);

	if (yum_entries == 0) {
		LOG(INFO, "No yum configuration entires. "
			  "Node will not install containers\n");
		goto out_clear;
	}

	alloc_size = sizeof(struct yum_cfg_list) +
		     (sizeof(struct yum_config)*yum_entries);

	
	repos = calloc(1, alloc_size);
	if (!repos) {
		LOG(INFO, "No memory to report yum configuration\n");
		goto out_clear;
	}

	repos->cnt = yum_entries;

	/*
 	 * Loop through the result and copy out the table data
 	 * Column 0 is the repo name
 	 * Column 1 is the repo url
 	 */
	for (yum_entries = 0; yum_entries < repos->cnt; yum_entries++) {
		repos->list[yum_entries].name =
			strdup(PQgetvalue(result, yum_entries, 0));
		repos->list[yum_entries].url =
			strdup(PQgetvalue(result, yum_entries, 1));
	}

out_clear:
	PQclear(result);
out:		
	return repos;
}

static void pg_free_yum_cfg(struct yum_cfg_list *repos)
{
	int i;
	for(i=0; i < repos->cnt; i++) {
		free((char *)repos->list[i].name);
		free((char *)repos->list[i].url);
	}
	free(repos);
}

static int pg_send_raw_sql(const char *sql,
		           const struct agent_config *acfg)
{
	struct postgres_info *info = acfg->db.db_priv;
	PGresult *result;
	ExecStatusType rc;
	int retc = -EINVAL;

	result = PQexec(info->conn, sql);

	rc = PQresultStatus(result);
	if (rc != PGRES_COMMAND_OK) {
		LOG(ERROR, "Unable to execute sql: %s\n",
			PQresultErrorMessage(result));
		goto out;
	}

	retc = 0;
out:
	return retc;
}

static int pg_show_table(const char *table,
			 const char *cols,
			 const char *filter,
			 int (*show_table_entry)(const struct tbl_entry *),
			 const struct agent_config *acfg)
{
	struct postgres_info *info = acfg->db.db_priv;
	int ret = -EINVAL;
	PGresult *result;
	ExecStatusType rc;
	int row, col, r, c;
	char sql[1024];
	struct tbl_entry elem;

	sprintf(sql, "SELECT %s from %s WHERE %s", cols, table, filter);
	result = PQexec(info->conn, sql);

	rc = PQresultStatus(result);
	if (rc != PGRES_TUPLES_OK) {
		LOG(ERROR, "Unable to query %s table: %s\n",
			table, PQresultErrorMessage(result));
		goto out;
	}

	row = PQntuples(result);
	col = PQnfields(result);

	/*
 	 * Loop through the result and copy out the table data
 	 * Column 0 is the repo name
 	 * Column 1 is the repo url
 	 */
	for (r = 0; r < row; r++) {
		elem.row = r;
		for (c = 0; c < col; c++) {
			elem.col = c;
			elem.tbl_value = PQgetvalue(result, r, c);
			ret = show_table_entry(&elem);
			if (ret)
				goto out_clear;
		}	
	}

	ret = 0;

out_clear:
	PQclear(result);
out:		
	return ret;
}

struct db_api postgres_db_api = {
	.init = pg_init,
	.cleanup = pg_cleanup,
	.connect = pg_connect,
	.disconnect = pg_disconnect,
	.get_yum_cfg = pg_get_yum_cfg,
	.free_yum_cfg = pg_free_yum_cfg,
	.send_raw_sql = pg_send_raw_sql,
	.show_table = pg_show_table,
};
