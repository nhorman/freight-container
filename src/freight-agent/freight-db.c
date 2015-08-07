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
 * *File: freight-db.c
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description API for DB connections
 * *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <alloca.h>
#include <string.h>
#include <freight-common.h>
#include <freight-log.h>
#include <freight-db.h>

struct db_api *api = NULL;

static const char* channel_map[] = {
	[CHAN_CONTAINERS] = "containers"
};

struct channel_callback {
	enum event_rc (*hndl)(const enum listen_channel chnl, const char *extra, const struct agent_config *acfg);
	enum listen_channel chnl;
	const struct agent_config *acfg;
	struct channel_callback *next;
};

static struct channel_callback *callbacks = NULL;

static int __chn_subscribe(const struct agent_config *acfg,
		    const char *lcmd,
		    const char *chnl)

{
	char *sql = strjoina(lcmd, " ", chnl, NULL);

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	return api->send_raw_sql(sql, acfg);
}

int channel_subscribe(const struct agent_config *acfg,
		      const enum listen_channel chn,
		      enum event_rc (*hndl)(const enum listen_channel chnl, const char *extra, const struct agent_config *acfg))
{

	struct channel_callback *tmp;
	char *chname;
	int rc;
	struct tbl *table;
	int i;

	tmp = callbacks;
	/*
	 * Look for callbacks already subscribed to this channel
	 */
	while (tmp) {
		if (tmp->chnl == chn)
			break;
		tmp = tmp->next;
	}

	if (tmp)
		return -EBUSY;

	tmp = calloc(1, sizeof(struct channel_callback));
	if (!tmp)
		return -ENOMEM;

	tmp->hndl = hndl;
	tmp->chnl = chn;
	tmp->acfg = acfg;
	tmp->next = callbacks;
	callbacks = tmp;

	table = get_tennants_for_host(acfg->cmdline.hostname, acfg);

	/*
	 * We actually want to subscribe to n channels here, <name>-<hostname>
	 * channel, and the <name>-<tennant> channel, for each tennant that this
	 * node is subscribed to.
	 */
	chname = strjoina("\"", channel_map[chn],"-", acfg->cmdline.hostname, "\"", NULL);
	rc = __chn_subscribe(acfg, "LISTEN", chname);

	for (i=0; i < table->rows; i++) {
		chname = strjoin("\"", channel_map[chn], "-", table->value[i][1], "\"", NULL);
		rc |= __chn_subscribe(acfg, "LISTEN", chname);
		free(chname);
	}

	free_tbl(table);

	/*
	 * once we are subscribed, make a dummy call to the handler for an initial table scan
	 */
	if (!rc)
		hndl(chn, NULL, acfg);

	return rc;
}


void channel_unsubscribe(const struct agent_config *acfg,
			 const enum listen_channel chn)
{
	struct channel_callback *tmp, *prev;
	char *chname;
	struct tbl *table;
	int i;

	tmp = prev = callbacks;

	while (tmp) {
		if (tmp->chnl == chn)
			break;
		prev = tmp;
		tmp = tmp->next;
	}

	if (!tmp)
		return;

	prev->next = tmp->next;

	chname = strjoina("\"", channel_map[chn],"-", acfg->cmdline.hostname, "\"", NULL);
	__chn_subscribe(acfg, "UNLISTEN", chname);

	table = get_tennants_for_host(acfg->cmdline.hostname, acfg);

	for (i=0; i < table->rows; i++) {
		chname = strjoin("\"", channel_map[chn], "-", table->value[i][1], "\"", NULL);
		__chn_subscribe(acfg, "UNLISTEN", chname);
		free(chname);
	}

	free_tbl(table);
}

enum event_rc event_dispatch(const char *chn, const char *extra)
{
	struct channel_callback *tmp;

	tmp = callbacks;
	while (tmp) {
		if (!strncmp(channel_map[tmp->chnl], chn, strlen(channel_map[tmp->chnl])))
			return tmp->hndl(tmp->chnl, extra, tmp->acfg);
		tmp = tmp->next;
	}

	return EVENT_NOCHAN;
}


struct tbl *alloc_tbl(int rows, int cols)
{
	int r;

	struct tbl *table = calloc(1, sizeof(struct tbl));
	if (!table)
		goto out;
	table->rows = rows;
	table->cols = cols;

	table->value = calloc(1, sizeof(char **)*rows);
	if (!table->value)
		goto out_free_table;

	for(r = 0; r < rows; r++) {
		table->value[r] = calloc(1, sizeof(char *)*cols);
		if (!table->value[r])
			goto out_free_cols;
	}

	return table;

out_free_cols:
	for (r = 0; r < rows; r++ )
		free(table->value[r]);
	free(table->value);
out_free_table:
	free(table);
	table = NULL;
out:
	return table;
}

void free_tbl(struct tbl *table)
{
	int i, j;

	if (!table)
		return;

	for (i = 0; i < table->rows; i++)
		for (j = 0; j < table->cols; j++)
			free(table->value[i][j]);

	for (i = 0; i < table->rows; i++)
		free(table->value[i]);

	free(table->value);
	free(table);
}

int add_repo(const char *name, const char *url,
	     const struct agent_config *acfg)
{
	char *sql; 

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO yum_config VALUES ('", name, "', '",
			url, "', '", acfg->db.user, "')", NULL);

	return api->send_raw_sql(sql, acfg);
}

extern int del_repo(const char *name,
		    const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM yum_config WHERE name = '", name, 
		"' AND tennant='", acfg->db.user, "'", NULL);

	return api->send_raw_sql(sql, acfg);
}


int add_host(const char *hostname,
	     const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO nodes VALUES ('", hostname, "', 'offline')", NULL);

	return api->send_raw_sql(sql, acfg);
}

int del_host(const char *hostname,
	     const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM nodes WHERE hostname = '", hostname, "'", NULL);

	return api->send_raw_sql(sql, acfg);
}

int subscribe_host(const char *tenant,
		   const char *host,
		   const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO tennant_hosts VALUES('", tenant, "', '", host, "')", NULL);

	return api->send_raw_sql(sql, acfg);
}

int unsubscribe_host(const char *tenant,
		     const char *host,
		     const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM tennant_hosts WHERE hostname = '", 
			host,"' AND tennant = '", tenant, "'", NULL);

	return api->send_raw_sql(sql, acfg);
}


int list_subscriptions(const char *tenant,
		       const struct agent_config *acfg)
{
	char *filter;
	struct tbl *table;
	int r;
	const char *real_tenant = tenant ?: acfg->db.user;

	if (!api->get_table)
		return -EOPNOTSUPP;

	filter = strjoina("tennant = '", real_tenant, "'", NULL);

	table = api->get_table("tennant_hosts", "*", filter, acfg);
	if (!table)
		LOGRAW("\nNo hosts subscribed to tennant %s\n", real_tenant);
	else {
		LOGRAW("\nHosts subscribed to tennant %s:\n", real_tenant);
		for (r = 0; r < table->rows; r ++)	
			LOGRAW("\t%s\n", table->value[r][0]);
	}

	free_tbl(table);
	return 0;
	
}

extern int change_host_state(const char *host, const char *newstate,
			     const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("UPDATE nodes SET state = '", newstate,
		       "' WHERE hostname = '", host, "'");

	return api->send_raw_sql(sql, acfg);
}

struct tbl* get_tennants_for_host(const char *host,
			  	  const struct agent_config *acfg)
{
	char *filter;

	if (!api->get_table)
		return NULL;

	filter = strjoina("hostname = '", host, "'", NULL);

	return api->get_table("tennant_hosts", "*", filter, acfg);
}

struct tbl* get_repos_for_tennant(const char *tenant,
				  const struct agent_config *acfg)
{
	char *filter;

	if (!api->get_table)
		return NULL;

	filter = strjoina("tennant='", tenant, "'", NULL);

	return api->get_table("yum_config", "name, url", filter, acfg);
}

int request_create_container(const char *cname,
                             const char *iname,
                             const char *chost,
			     const struct agent_config *acfg)
{
	char *sql;
	int rc;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO containers VALUES(",
		       "'", acfg->db.user, "',",
		       "'", iname, "',",
		       "'", cname, "',",
		       "'", chost, "',",
		       "'new')", NULL);

	rc = api->send_raw_sql(sql, acfg);

	if (rc)
		return rc;

	/*
	 * If we add the sql safely, then we need to wake someone up to read the table
	 * If a chost is specified, then notify that host only, otherwise, notify the 
	 * master to pick a host for us
	 */
	if (chost)
		rc = notify_host(CHAN_CONTAINERS, chost, acfg);
	else
		LOG(INFO, "NEED TO IMPLEMENT MASTER FUNCTIONALITY HERE\n");

	return rc;

}

int notify_host(const enum listen_channel chn, const char *host,
		const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("NOTIFY \"", channel_map[chn], "-", host, "\"");

	return api->send_raw_sql(sql, acfg);
} 
