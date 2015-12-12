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
 * *File: sqlite-db.c
 * *
 * *Author:Neil Horman
 * *
 * *Date:
 * *
 * *Description implements access to sqlite db 
 * *********************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <semaphore.h>
#include <freight-common.h>
#include <freight-db.h>


struct sqlite_info {
	sqlite3 *conn;
};

static int sq_init(struct agent_config *acfg)
{
	struct sqlite_info *info = calloc(1, sizeof(struct sqlite_info));
	if (!info)
		return -ENOMEM;
	acfg->db.db_priv = info;
	return 0;
}

static void sq_cleanup(struct agent_config *acfg)
{
	struct sqlite_info *info = acfg->db.db_priv;

	free(info);
	acfg->db.db_priv = NULL;
	return;
}

static int sq_disconnect(struct agent_config *acfg)
{
	struct sqlite_info *info = acfg->db.db_priv;
	sqlite3_close_v2(info->conn);
	return 0;
}

static int sq_connect(struct agent_config *acfg)
{
	struct sqlite_info *info = acfg->db.db_priv;

	return sqlite3_open_v2(acfg->db.dbname, &info->conn,
			       SQLITE_OPEN_READWRITE, NULL);
}


static int sq_send_raw_sql(const char *sql,
		           const struct agent_config *acfg)
{
	sqlite3_stmt *stmt;
	struct sqlite_info *info = acfg->db.db_priv;
	int rc;

	rc = sqlite3_prepare_v2(
				info->conn,
				sql, strlen(sql),
				&stmt, NULL);

	if (rc != SQLITE_OK)
		goto out;

	while (rc != SQLITE_DONE)
		rc = sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	if (rc == SQLITE_DONE)
		rc = 0;
out:
	return rc;
}

static struct tbl* sq_get_table(enum db_table type,
			 const char *cols,
			 const char *filter,
			 const struct agent_config *acfg)
{
	struct sqlite_info *info = acfg->db.db_priv;
	int rc;
	int row, col, r, c;
	char *sql;
	const char *table; 
	sqlite3_stmt *stmtc, *stmt;
	struct tbl *rtable = NULL;
	const char *tmp;

	table = get_tablename(type);
	sql = strjoin("SELECT ", cols, " FROM ", table, " WHERE ", filter, NULL);

	rc = sqlite3_prepare_v2(info->conn, sql, strlen(sql),
				&stmt, NULL);
	if (rc != SQLITE_OK)
		goto out;

	sql = strjoin("SELECT COUNT(*) FROM ", table, " WHERE " , filter, NULL);

	rc = sqlite3_prepare_v2(info->conn, sql, strlen(sql),
				&stmtc, NULL);

	if (rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		goto out;
	}


	rc = sqlite3_step(stmt);
	
	row = sqlite3_column_int(stmt, 0); 
	col = sqlite3_column_count(stmt); 

	rtable = alloc_tbl(row, col, type);
	if (!rtable)
		goto out_clear;
	
	/*
 	 * Loop through the result and copy out the table data
 	 * Column 0 is the repo name
 	 * Column 1 is the repo url
 	 */
	for (r = 0; r < row; r++) { 
		for (c = 0; c < col; c++) {
			tmp = (const char *)sqlite3_column_text(stmt, c);
			rtable->value[r][c] = strdup(tmp);
			if (!rtable->value[r][c]) {
				free_tbl(rtable);
				rtable = NULL;
				goto out_clear;
			}
		}
		sqlite3_step(stmt);
	}


out_clear:
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmtc);
out:		
	return rtable;
}

static enum event_rc sq_poll_notify(const struct agent_config *acfg)
{
	return EVENT_CONSUMED;
}

static int sq_notify(enum notify_type type, enum listen_channel chn,
                     const char *name, const struct agent_config *acfg)
{
	char *sql = strjoina("NOTIFY \"", name, "\"", NULL);

	return pg_send_raw_sql(sql, acfg);
}

struct db_api sqlite_db_api = {
	.init = sq_init,
	.cleanup = sq_cleanup,
	.connect = sq_connect,
	.disconnect = sq_disconnect,
	.send_raw_sql = sq_send_raw_sql,
	.get_table = sq_get_table,
	.poll_notify = sq_poll_notify,
};
