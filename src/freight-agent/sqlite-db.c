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
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <freight-common.h>
#include <freight-db.h>


struct sqlite_info {
	sqlite3 *conn;
	sem_t *notify;
};

static int sq_send_raw_sql(const char *sql,
			   const struct agent_config *acfg);

static int sq_delete_from_table(enum db_table type, const char *filter, 
				const struct agent_config *acfg)
{
	char *sql;
	const char *tname = get_tablename(type);

	sql = strjoina("DELETE from ", tname, " WHERE ", filter, NULL);

	return sq_send_raw_sql(sql, acfg);
}

static int sq_init(struct agent_config *acfg)
{
	struct sqlite_info *info = calloc(1, sizeof(struct sqlite_info));
	if (!info)
		return -ENOMEM;
	acfg->db.db_priv = info;
	info->notify = sem_open("/freightsem", O_CREAT|O_RDWR, 0);
	return 0;
}

static void sq_cleanup(struct agent_config *acfg)
{
	struct sqlite_info *info = acfg->db.db_priv;

	sem_close(info->notify);
	info->notify = NULL;
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
	int rc;

	struct sqlite_info *info = acfg->db.db_priv;

	rc = sqlite3_open_v2(acfg->db.dbname, &info->conn,
			       SQLITE_OPEN_READWRITE, NULL);

	if (info->notify == SEM_FAILED) {
		LOG(ERROR, "Could not create semaphore\n");
		sqlite3_close_v2(info->conn);
		rc = -EINVAL;
	}

	/*
	 * Empty the event table 
	 */
	sq_delete_from_table(TABLE_EVENTS, "rowid > 0", acfg);
	return rc;
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

	if (filter)
		sql = strjoin("SELECT ", cols, " FROM ", table, " WHERE ", filter, NULL);
	else
		sql = strjoin("SELECT ", cols, " FROM ", table, NULL);

	rc = sqlite3_prepare_v2(info->conn, sql, strlen(sql),
				&stmt, NULL);
	if (rc != SQLITE_OK)
		goto out;

	if (filter)
		sql = strjoin("SELECT COUNT(*) FROM ", table, " WHERE " , filter, NULL);
	else
		sql = strjoin("SELECT COUNT(*) FROM ", table, NULL);

	rc = sqlite3_prepare_v2(info->conn, sql, strlen(sql),
				&stmtc, NULL);

	if (rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		goto out;
	}

	sqlite3_step(stmtc);
	
	row = sqlite3_column_int(stmtc, 0); 
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
		sqlite3_step(stmt);
		for (c = 0; c < col; c++) {
			tmp = (const char *)sqlite3_column_text(stmt, c);
			rtable->value[r][c] = strdup(tmp);
			if (!rtable->value[r][c]) {
				free_tbl(rtable);
				rtable = NULL;
				goto out_clear;
			}
		}
	}


out_clear:
	sqlite3_finalize(stmt);
	sqlite3_finalize(stmtc);
out:		
	return rtable;
}

static enum event_rc sq_poll_notify(const struct agent_config *acfg)
{
	struct sqlite_info *info = acfg->db.db_priv;
	struct tbl *events;
	enum event_rc ev_rc;

	if (sem_wait(info->notify) < 0)
	{
		/*
		 * getting EINTR is how we end this loop properly
		 */
		if (errno != EINTR)
			LOG(ERROR, "sem_wait() failed: %s\n", strerror(errno));
		return EVENT_INTR;
	}

	events = sq_get_table(TABLE_EVENTS, "*", "rowid=1", acfg);
	sq_delete_from_table(TABLE_EVENTS, "rowid=1", acfg);

	LOG(DEBUG, "GOT AN EVENT NOTIFICATION\n");
	if (events->rows != 0) {
		ev_rc = event_dispatch(lookup_tbl(events, 0, COL_NAME),
				       lookup_tbl(events, 0, COL_URL));
		if (ev_rc != EVENT_CONSUMED)
			LOG(ERROR, "EVENT was not properly consumed\n");
	}
	
	free_tbl(events);
		
	return EVENT_CONSUMED;
}

static int sq_notify(enum notify_type type, enum listen_channel chn,
                     const char *name, const struct agent_config *acfg)
{
	int rc;
	struct sqlite_info *info = acfg->db.db_priv;
	char *sql = strjoina("INSERT INTO event_table VALUES (",
			     "'", name, "','')", NULL);
	rc = sq_send_raw_sql(sql, acfg);
	if (rc)
		return rc;
	return sem_post(info->notify);
}

static int sq_subscribe(const char *lcmd, const char *chnl, const struct agent_config *acfg)
{
	return 0;
}

static int sq_table_op(enum table_op op, enum db_table tbl, const struct colvallist *setlist,
		       const struct colvallist *filter, const struct agent_config *acfg)
{
	const char *tblname = get_tablename(tbl);
	char *sql;
	int i, rc;

	switch (op) {

	case OP_INSERT:
		sql = strjoin("INSERT INTO ", tblname, " VALUES (", NULL);
		break;
	case OP_DELETE:
		sql = strjoin("DELETE FROM ", tblname, " WHERE ", NULL);
		break;
	case OP_UPDATE:
		sql = strjoin("UPDATE ", tblname, " SET ", NULL);
		if (setlist->count > 1)
			sql = strappend(sql, "(", NULL);
		break;
	default:
		LOG(ERROR, "Unknown table operation\n");
		return -ENOENT;
	}

	switch (op) {

	case OP_UPDATE:
		/* FALLTHROUGH */
	case OP_INSERT:
		/*
		 * Note, this expects entries to be in column order!
		 */
		for(i=0; i < setlist->count; i++) { 
			if (op == OP_UPDATE)
				sql = strappend(sql, get_colname(tbl, setlist->entries[i].column), " = ", NULL);
			if (setlist->entries[i].value)
				sql = strappend(sql, "'", setlist->entries[i].value, "'", NULL);
			else
				sql = strappend(sql, "null", NULL);
			if (i != setlist->count-1)
				sql = strappend(sql, ", ", NULL);
		}
		if ((op == OP_INSERT) && (setlist->count > 1))
			sql = strappend(sql, ")", NULL);
		if (op == OP_UPDATE)
			sql = strappend(sql, " WHERE ", NULL);
		else
			break;

		/* FALLTHROUGH AGAIN FOR OP_UPDATE */
	case OP_DELETE:
		for(i=0; i < filter->count; i++) {
			if (filter->entries[i].column == COL_VERBATIM)
				sql = strappend(sql, filter->entries[i].value, NULL);
			else
				sql = strappend(sql, get_colname(tbl, filter->entries[i].column),
						"='", filter->entries[i].value, "'", NULL);
			if (i < filter->count-1)
				sql = strappend(sql, " AND ", NULL);
		}
		break;
	default:
		break;
	}

	rc = sq_send_raw_sql(sql, acfg);

	free(sql);
	return rc;
}


struct db_api sqlite_db_api = {
	.init = sq_init,
	.cleanup = sq_cleanup,
	.connect = sq_connect,
	.disconnect = sq_disconnect,
	.table_op = sq_table_op,
	.send_raw_sql = sq_send_raw_sql,
	.get_table = sq_get_table,
	.notify = sq_notify,
	.subscribe = sq_subscribe,
	.poll_notify = sq_poll_notify,
};
