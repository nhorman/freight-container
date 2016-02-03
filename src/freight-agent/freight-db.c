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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <freight-common.h>
#include <freight-log.h>
#include <freight-db.h>

struct db_api *api = NULL;

static const char* channel_map[] = {
	[CHAN_CONTAINERS] = "containers",
	[CHAN_CONTAINERS_SCHED] = "container-sched",
	[CHAN_TENNANT_HOSTS] = "tennant_hosts",
	[CHAN_GLOBAL_CONFIG] = "global_config",
	[CHAN_NODES] = "nodes"
};

static char *tablenames[TABLE_MAX] = {
        [TABLE_TENNANTS] = "tennants",
        [TABLE_NODES] = "nodes",
        [TABLE_TENNANT_HOSTS] = "tennant_hosts",
        [TABLE_YUM_CONFIG] = "yum_config",
        [TABLE_CONTAINERS] = "containers",
	[TABLE_NETWORKS] = "networks",
	[TABLE_NETMAP] = "net_container_map",
	[TABLE_EVENTS] = "event_table",
	[TABLE_GCONF] = "global_config",
	[TABLE_ALLOCMAP]= "net_address_allocation_map"
};

static char *colnames[TABLE_MAX][COL_MAX] = {
	/*TABLE_TENNANTS*/
	{"tennant", NULL, NULL, NULL, NULL, NULL, NULL, "proxypass", NULL, "proxyadmin", NULL, },
	/*TABLE_NODES*/
	{NULL, "hostname", "state", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "load", "modified", },
	/*TABLE_TENNANT_HOSTS*/
	{"tennant", "hostname", },
	/*TABLE_YUM_CONFIG*/
	{"tennant", NULL, NULL, "name", "url", },
	/*TABLE_CONTAINERS*/
	{"tennant", "hostname", "state", NULL, NULL, "iname", "cname", },
	/*TABLE_NETWORKS*/
	{"tennant", NULL, "state", "name", NULL, NULL, NULL, "config", },
	/*TABLE_NETMAP*/
	{"tennant", NULL, NULL, NULL, NULL, "name", "network", },
	/*TABLE_EVENTS*/
	{NULL, NULL, NULL, "event", "extra", },
	/*TABLE_GCONF*/
	{NULL, NULL, NULL, "key", NULL, NULL, NULL, NULL, "value", },
	/*TABLE_ALLOCMAP*/
	{"tennant", "ownerhost", "allocated", "ownerip", NULL, "type", NULL, NULL, NULL, "address", },
	
};

/*
 * This table maps the human readable column names
 * to the numeric columns that each table returns.
 * The array indicies are:
 * TENNANT, HOSTNAME, STATE, NAME, URL, INAME, CNAME, PROXYPASS, TYPE, CONFIG PROXYADMIN LOAD MODIFIED
 * Note: the net_contaienr_map uses INAME for the container name and CNAME for the network name
 * Note: the global_config table uses NAME for the key and CONFIG for the value column
 * Note: The allocmap uses INAME for the ownerip
 */

static int db_col_map[TABLE_MAX][COL_MAX] = {
 [TABLE_TENNANTS] =		{ 0, -1, -1, -1, -1, -1, -1,  1, -1,  2, -1, -1},
 [TABLE_NODES] =		{-1,  0,  1, -1, -1, -1, -1, -1, -1, -1,  2,  3},
 [TABLE_TENNANT_HOSTS] = 	{ 1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
 [TABLE_YUM_CONFIG] =		{ 2, -1, -1,  0,  1, -1, -1, -1, -1, -1, -1, -1},
 [TABLE_CONTAINERS] =		{ 0,  3,  4, -1, -1,  1,  2, -1, -1, -1, -1, -1},
 [TABLE_NETWORKS] =		{ 1, -1,  2,  0, -1, -1, -1, -1,  3, -1, -1, -1},
 [TABLE_NETMAP] =		{ 0, -1, -1, -1, -1,  1,  2, -1, -1, -1, -1, -1}, 
 [TABLE_EVENTS] =		{ -1,-1, -1,  0, -1, -1, -1, -1,  1, -1, -1, -1},
 [TABLE_GCONF] =		{ -1,-1, -1,  0, -1, -1, -1, -1,  1, -1, -1, -1},
 [TABLE_ALLOCMAP] =		{ 1, 5,  3,  4,  3, -1,  -1, -1, -1,  2, -1, -1} 
};

struct channel_callback {
	enum event_rc (*hndl)(const enum listen_channel chnl, const char *extra, const struct agent_config *acfg);
	enum listen_channel chnl;
	const struct agent_config *acfg;
	struct channel_callback *next;
};

static struct channel_callback *callbacks = NULL;

static int auto_detach_networks_from_container(const char *iname, const char *tennant, const struct agent_config *acfg);

static int __chn_subscribe(const struct agent_config *acfg,
		    const char *lcmd,
		    const char *chnl)

{
	if (!api->subscribe)
		return -EOPNOTSUPP;
	return api->subscribe(lcmd, chnl, acfg);
}

int channel_add_tennant_subscription(const struct agent_config *acfg,
				     const enum listen_channel chn,
				     const char *tennant)
{
	char *chname;
	struct channel_callback *idx;


	/*
	 * need to ensure that we already have a subscription
	 */
	for (idx = callbacks; idx != NULL; idx = idx->next) {
		if (idx->chnl == chn)
			goto update;
	}
	return -ENOENT;

update:
	chname = strjoin("\"", channel_map[chn], "-", tennant, "\"", NULL);
	
	return __chn_subscribe(acfg, "LISTEN", chname);
}

int channel_del_tennant_subscription(const struct agent_config *acfg,
				     const enum listen_channel chn,
				     const char *tennant)
{
	char *chname;

	chname = strjoin("\"", channel_map[chn], "-", tennant, "\"", NULL);
	
	return __chn_subscribe(acfg, "UNLISTEN", chname);
}

int channel_subscribe(const struct agent_config *acfg,
		      const enum listen_channel chn,
		      enum event_rc (*hndl)(const enum listen_channel chnl, const char *extra, const struct agent_config *acfg))
{

	struct channel_callback *tmp;
	char *chname, *achname;
	int rc = 0;

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


	/*
	 * We actually want to subscribe to 2 channels here, <name>-<hostname>
	 * channel, and the <name>-all channel, if a signaler wants to reach all hosts 
	 * NOTE: CHAN_CONTAINERS_SCHED is special, as it only has an -all channel
	 */
	chname = strjoina("\"", channel_map[chn],"-", acfg->cmdline.hostname, "\"", NULL);
	if (chn != CHAN_CONTAINERS_SCHED)
		rc = __chn_subscribe(acfg, "LISTEN", chname);

	if (!rc) {
		achname = strjoina("\"", channel_map[chn], "-all\"", NULL);
		rc = __chn_subscribe(acfg, "LISTEN", achname);
	}

	if (rc)
		__chn_subscribe(acfg, "UNLISTEN", chname);

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
	char *chname, *achname;

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

	if (chn != CHAN_CONTAINERS_SCHED) {
		chname = strjoina("\"", channel_map[chn],"-", acfg->cmdline.hostname, "\"", NULL);
		__chn_subscribe(acfg, "UNLISTEN", chname);
	}

	achname = strjoina("\"", channel_map[chn], "-all\"", NULL);
	__chn_subscribe(acfg, "UNLISTEN", achname);
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


const char* get_tablename(enum db_table id)
{
	return tablenames[id];
}

const enum db_table get_tableid(const char *name)
{
	int i;

	for (i=0; i < TABLE_MAX; i++) {
		if (!strcmp(name, tablenames[i]))
			return i;
	}

	return TABLE_MAX;
}

const char *get_colname(enum db_table tbl, enum table_col col)
{
	return colnames[tbl][col];
}

struct tbl *alloc_tbl(int rows, int cols, enum db_table type)
{
	int r;

	struct tbl *table = calloc(1, sizeof(struct tbl));
	if (!table)
		goto out;
	table->rows = rows;
	table->cols = cols;
	table->type = type;

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

int is_tbl_empty(struct tbl *table)
{
	return (table->rows == 0);
}

void * lookup_tbl(struct tbl *table, int row, enum table_col col)
{
	if ((long long)(table->value[row][db_col_map[table->type][col]]) == -1)
		return NULL;

	return table->value[row][db_col_map[table->type][col]];
}

char* get_tennant_proxy_pass(const char *user, const struct agent_config *acfg)
{
	struct tbl *table;
	char *filter;
	char *pass = NULL;

	if (!api->get_table)
		return NULL;

	filter = strjoina("tennant = '", user, "'", NULL);

	table = api->get_table(TABLE_TENNANTS, "*", filter, acfg);

	if (!table->rows)
		goto out;

	pass = lookup_tbl(table, 0, COL_PROXYPASS);

	if (!pass)
		goto out;

	pass = strdup(pass);
out:
	free_tbl(table);
	return pass;
}

int get_tennant_proxy_admin(const char *user, const struct agent_config *acfg)
{
	struct tbl *table;
	char *filter;
	char *admin = NULL;
	int rc = 0;

	if (!api->get_table)
		return 0;

	filter = strjoina("tennant = '", user, "'", NULL);

	table = api->get_table(TABLE_TENNANTS, "*", filter, acfg);

	if (!table->rows)
		goto out;

	admin = lookup_tbl(table, 0, COL_PROXYADMIN);

	if (!admin)
		goto out;

	if (!strncasecmp(admin, "t", 1))
		rc = 1;
out:
	free_tbl(table);
	return rc;
}
static int old_add_repo(const char *name, const char *url,
	     const char *tennant,
	     const struct agent_config *acfg)
{
	char *sql; 

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO yum_config VALUES ('", name, "', '",
			url, "', '", tennant, "')", NULL);

	return api->send_raw_sql(sql, acfg);
}

int add_repo(const char *name, const char *url,
	     const char *tennant, const struct agent_config *acfg)
{
	struct colval values[3];
	struct colvallist list;

	if (!api->table_op)
		return old_add_repo(name, url, tennant, acfg);

	list.count = 3;
	list.entries = values;
	values[0].value = name;
	values[0].column = COL_NAME;
	values[1].value = url;
	values[1].column = COL_URL;
	values[2].value = tennant;
	values[2].column = COL_TENNANT;

	return api->table_op(OP_INSERT, TABLE_YUM_CONFIG, &list, NULL, acfg);
}

static int old_del_repo(const char *name,
		    const char *tennant,
		    const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM yum_config WHERE name = '", name, 
		"' AND tennant='", tennant, "'", NULL);

	return api->send_raw_sql(sql, acfg);
}

int del_repo(const char *name,
	     const char *tennant,
	     const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[2];

	if (!api->table_op)
		return old_del_repo(name, tennant, acfg);

	list.count = 2;
	list.entries = values;

	values[0].column = COL_NAME;
	values[0].value = name;
	values[1].column = COL_TENNANT;
	values[1].value = tennant;

	return api->table_op(OP_DELETE, TABLE_YUM_CONFIG, NULL, &list, acfg);
}

static int old_add_host(const char *hostname,
	                const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO nodes VALUES ('", hostname, "', 'offline', 0, null)", NULL);

	return api->send_raw_sql(sql, acfg);
}

int add_host(const char *hostname,
	     const struct agent_config *acfg)
{
	struct colval values[4];
	struct colvallist list;
	if (!api->table_op)
		return old_add_host(hostname, acfg);

	list.count = 4;
	list.entries = values;
	values[0].column = COL_HOSTNAME;
	values[0].value = hostname;
	values[1].column = COL_STATE;
	values[1].value = "offline";
	values[2].column = COL_LOAD;
	values[2].value = "0";
	values[3].column = COL_MODIFIED;
	values[3].value = NULL;

	return api->table_op(OP_INSERT, TABLE_NODES, &list, NULL, acfg);
}

static int old_del_host(const char *hostname,
	     const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM nodes WHERE hostname = '", hostname, "'", NULL);

	return api->send_raw_sql(sql, acfg);
}

int del_host(const char *hostname, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[1];

	if (!api->table_op)
		return old_del_host(hostname, acfg);

	list.count = 1;
	list.entries = values;
	values[0].column = COL_HOSTNAME;
	values[0].value = hostname;

	return api->table_op(OP_DELETE, TABLE_NODES, NULL, &list, acfg);
	
}

int old_subscribe_host(const char *host,
		       const char *tennant,
		       const struct agent_config *acfg)
{
	char *sql;
	int rc;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO tennant_hosts VALUES('", host, "', '", tennant, "')", NULL);

	rc = api->send_raw_sql(sql, acfg);
	if (rc)
		return rc;

	return notify_host(CHAN_TENNANT_HOSTS, host, acfg);
}

int subscribe_host(const char *host,
		   const char *tennant,
		   const struct agent_config *acfg)
{
	int rc;
	struct colval values[2];
	struct colvallist list;

	if (!api->table_op)
		return old_subscribe_host(host, tennant, acfg);

	list.count = 2;
	list.entries = values;
	values[0].column = COL_HOSTNAME;
	values[0].value = host;
	values[1].column = COL_TENNANT;
	values[1].value = tennant;

	rc = api->table_op(OP_INSERT, TABLE_TENNANT_HOSTS, &list, NULL, acfg);
	
	if (!rc)
		rc = notify_host(CHAN_TENNANT_HOSTS, host, acfg);

	return rc;
}

static int old_unsubscribe_host(const char *tenant,
		     const char *host,
		     const struct agent_config *acfg)
{
	char *sql;
	int rc;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM tennant_hosts WHERE hostname = '", 
			host,"' AND tennant = '", tenant, "'", NULL);

	rc = api->send_raw_sql(sql, acfg);
	if (rc)
		return rc;

	return notify_host(CHAN_TENNANT_HOSTS, host, acfg);
}

int unsubscribe_host(const char *tennant,
		     const char *host,
		     const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[2];

	if (!api->table_op)
		return old_unsubscribe_host(tennant, host, acfg);

	list.count = 2;
	list.entries = values;
	values[0].column = COL_HOSTNAME;
	values[0].value = host;
	values[1].column = COL_TENNANT;
	values[1].value = tennant;

	return api->table_op(OP_DELETE, TABLE_TENNANT_HOSTS, NULL, &list, acfg);
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

	table = api->get_table(TABLE_TENNANT_HOSTS, "*", filter, acfg);
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

static int old_change_host_state(const char *host, const char *newstate,
			     const struct agent_config *acfg)
{
	char *sql;
	int rc;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("UPDATE nodes SET state = '", newstate,
		       "' WHERE hostname = '", host, "'");

	rc = api->send_raw_sql(sql, acfg);
	if (!rc)
		notify_all(CHAN_NODES, acfg);
	return rc;
}

int change_host_state(const char *host, const char *newstate,
			     const struct agent_config *acfg)
{
	struct colvallist set, filter;
	struct colval sval[1], fval[1];
	int rc;

	if (!api->table_op)
		return old_change_host_state(host, newstate, acfg);

	set.count = 1;
	filter.count = 1;
	set.entries = sval;
	filter.entries = fval;

	sval[0].column = COL_STATE;
	sval[0].value = newstate;
	fval[0].column = COL_HOSTNAME;
	fval[0].value = host;

	rc = api->table_op(OP_UPDATE, TABLE_NODES, &set, &filter, acfg);
	if (!rc)
		notify_all(CHAN_NODES, acfg);
	return rc;
}

static int old_assign_container_host(const char *name, const char *host,
			  const char *tennant,
			  const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("UPDATE nodes SET (hostname='", host, ",state='staged') ",
		       "WHERE tennant='",tennant," AND iname='",name,"'", NULL);

	return api->send_raw_sql(sql, acfg);
}

int assign_container_host(const char *name, const char *host,
			  const char *tennant,
			  const struct agent_config *acfg)
{
	struct colvallist slist;
	struct colvallist flist;
	struct colval set[2];
	struct colval filter[2];

	if (!api->table_op)
		return old_assign_container_host(name, host, tennant, acfg);

	slist.count = flist.count = 2;
	slist.entries = set;
	flist.entries = filter;

	set[0].column = COL_HOSTNAME;
	set[0].value = host;
	set[1].column = COL_STATE;
	set[1].value = "staged";
	filter[0].column = COL_TENNANT;
	filter[0].value = tennant;
	filter[1].column = COL_INAME;
	filter[1].value = name;

	return api->table_op(OP_UPDATE, TABLE_NODES, &slist, &flist, acfg);
}

struct tbl* get_tennants_for_host(const char *host,
			  	  const struct agent_config *acfg)
{
	char *filter;

	if (!api->get_table)
		return NULL;

	filter = strjoina("hostname = '", host, "'", NULL);

	return api->get_table(TABLE_TENNANT_HOSTS, "*", filter, acfg);
}

struct tbl* get_repos_for_tennant(const char *tenant,
				  const struct agent_config *acfg)
{
	char *filter;

	if (!api->get_table)
		return NULL;

	filter = strjoina("tennant='", tenant, "'", NULL);

	return api->get_table(TABLE_YUM_CONFIG, "name, url", filter, acfg);
}

struct tbl* get_containers_for_host(const char *host,
				   const char *state,
				   const struct agent_config *acfg)
{
	char *filter;

	if (!api->get_table)
		return NULL;

	filter = strjoina("(hostname='", host, "' AND state='", state, "')", NULL);

	return api->get_table(TABLE_CONTAINERS, "*", filter, acfg);
}

static struct tbl *get_container_info(const char *iname,
				      const char *tennant,
                                     const struct agent_config *acfg)
{
	char *filter;

	if (!api->get_table)
		return NULL;

	filter = strjoina("(iname='", iname, "' AND tennant='", tennant,"')", NULL);

	return api->get_table(TABLE_CONTAINERS, "*", filter, acfg);
}

struct tbl *get_containers_of_type(const char *cname,
					  const char *tennant,
					  const char *host,
					  const struct agent_config *acfg)
{
	char *bfilter, *filter;

	if (!api->get_table)
		return NULL;

	bfilter = strjoina("(cname='", cname, "' AND tennant='", tennant, NULL);

	if (host)
		filter = strjoina(bfilter, "' AND hostname='", host, "')", NULL);
	else 
		filter = strjoina(bfilter, "')", NULL);

	return api->get_table(TABLE_CONTAINERS, "*", filter, acfg);
}

struct tbl *get_host_info(const char *name, const struct agent_config *acfg)
{
	char *filter = strjoina("hostname='", name, "'", NULL);

	return get_raw_table(TABLE_NODES, filter, acfg);
} 

static int old_request_create_container(const char *cname,
					const char *iname,
					const char *chost,
					const char *tennant,
					const struct agent_config *acfg)
{
	char *sql;
	int rc;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	if (chost)
		sql = strjoina("INSERT INTO containers VALUES(",
			       "'", tennant, "',",
			       "'", iname, "',",
			       "'", cname, "',",
			       "'", chost, "',",
			       "'staged')", NULL);
	else
		sql = strjoina("INSERT INTO containers VALUES(",
			       "'", tennant, "',",
			       "'", iname, "',",
			       "'", cname, "',",
			       " null, ",
			       "'assigning-host')", NULL);
	

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
		rc = notify_all(CHAN_CONTAINERS_SCHED, acfg);

	return rc;

}

int request_create_container(const char *cname,
			     const char *iname,
			     const char *chost,
			     const char *tennant,
			     const struct agent_config *acfg)
{

	struct colval values[5];
	struct colvallist list;
	int rc;

	if (!api->table_op)
		return old_request_create_container(cname, iname, chost, tennant, acfg);

	list.count = 5;
	list.entries = values;

	values[0].column = COL_TENNANT;
	values[0].value = tennant;
	
	values[1].column = COL_NAME;
	values[1].value = iname;

	values[2].column = COL_CNAME;
	values[2].value = cname;

	values[3].column = COL_HOSTNAME;
	values[3].value = chost;

	values[4].column = COL_STATE;
	values[4].value = chost ? "staged" : "assigning-host";

	rc = api->table_op(OP_INSERT, TABLE_CONTAINERS, &list, NULL, acfg);
	if (!rc) {
		/*
		 * If we add the sql safely, then we need to wake someone up to read the table
		 * If a chost is specified, then notify that host only, otherwise, notify the 
		 * master to pick a host for us
		 */
		if (chost)
			rc = notify_host(CHAN_CONTAINERS, chost, acfg);
		else
			rc = notify_all(CHAN_CONTAINERS_SCHED, acfg);
	}

	return rc;
}

static int old_request_delete_container(const char *iname,
			     const char *tennant,
			     const int force,
			     const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM containers WHERE tennant='",
		tennant, "' AND iname='",iname,
		"' AND state='failed' OR state='staged' OR state='assigning-host'", NULL);

	return api->send_raw_sql(sql, acfg);

}

int request_delete_container(const char *iname,
			     const char *tennant,
			     const int force,
			     const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[3];

	if (auto_detach_networks_from_container(iname, tennant, acfg)) {
		LOG(ERROR, "Unable to detatch container from some networks\n");
		return -EFAULT;
	}

	if (!api->table_op)
		return old_request_delete_container(iname, tennant, force, acfg);

	list.count = 3;
	list.entries = values;

	values[0].column = COL_TENNANT;
	values[0].value = tennant;
	values[1].column = COL_INAME;
	values[1].value = iname;
	values[2].column = COL_VERBATIM;
	values[2].value = "state='failed' OR state='staged' OR state='assigning-host'";

	return api->table_op(OP_DELETE, TABLE_CONTAINERS, NULL, &list, acfg);
}

extern int request_boot_container(const char *iname, const char *tennant,
				  const struct agent_config *acfg)
{
	int rc;
	struct tbl *container;
	char *host;

	container = get_container_info(iname, tennant, acfg);

	if (is_tbl_empty(container)) {
		LOG(WARNING, "Container %s does not exist\n", iname);
		rc = -EEXIST;
		goto out;
	}

	rc = change_container_state(tennant, iname, "staged",
	 			    "start-requested", acfg);

	if (rc)
		rc = change_container_state(tennant, iname, "failed",
				    "start-requested", acfg);

	if (rc)
		LOG(WARNING, "container %s is in the wrong state\n", iname);

		
	host = lookup_tbl(container, 0, COL_HOSTNAME);

	if (host) {
		if(!strcmp(host, "all"))
			rc = notify_tennant(CHAN_CONTAINERS, tennant, acfg);
		else
			rc = notify_host(CHAN_CONTAINERS, host, acfg);
	} else {
		LOG(INFO, "NEED TO IMPLEMENT MASTER FUNCTION FOR HOST SELECTION\n");
	}

	if (rc)
		LOG(WARNING, "Notificaion of host failed.  Boot may be delayed\n");

out:
	free_tbl(container);
	return rc; 
	
}

extern int request_poweroff_container(const char *iname,
				      const char *tennant,
				  const struct agent_config *acfg)
{
	int rc;
	struct tbl *container;

	rc = change_container_state(tennant, iname, "running",
				    "exiting", acfg);

	container = get_container_info(iname, tennant, acfg);

	rc = notify_host(CHAN_CONTAINERS, lookup_tbl(container, 0, COL_HOSTNAME), acfg);

	return rc;
}

int print_container_list(const char *tennant, 
			 const struct agent_config *acfg)
{
	struct tbl *containers;
	char *filter;
	int i;

	if (!api->get_table)
		return -EOPNOTSUPP;

	filter = strjoina("tennant='",tennant,"'",NULL);

	containers = get_raw_table(TABLE_CONTAINERS, filter, acfg);

	if (!containers || !containers->rows)
		return -ENOENT;

	LOGRAW("CONTAINER NAME   |   CONTAINER TYPE   |   STATE\n");
	LOGRAW("--------------------------------------------\n");
	for (i=0; i < containers->rows; i++) {
		LOGRAW("%-17s|%-20s|%-8s\n",
			(char *)lookup_tbl(containers, i, COL_INAME),
			(char *)lookup_tbl(containers, i, COL_CNAME),
			(char *)lookup_tbl(containers, i, COL_STATE));
	}

	free_tbl(containers);
	return 0;

}



static int old_change_container_state(const char *tennant,
                                  const char *iname,
				  const char *oldstate,
                                  const char *newstate,
                                  const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("UPDATE containers set state ='", newstate,
		       "' WHERE tennant = '", tennant,
		       "' AND iname = '", iname,
		       "' AND state = '", oldstate, "'", NULL);

	return api->send_raw_sql(sql, acfg);
}

int change_container_state(const char *tennant,
                                  const char *iname,
				  const char *oldstate,
                                  const char *newstate,
                                  const struct agent_config *acfg)
{
	struct colvallist slist;
	struct colvallist flist;
	struct colval set[1];
	struct colval filter[3];

	if (!api->table_op)
		return old_change_container_state(tennant, iname, oldstate, newstate, acfg);

	slist.count = 1;
	flist.count = 3;
	slist.entries = set;
	flist.entries = filter;

	set[0].column = COL_STATE;
	set[0].value = newstate;

	filter[0].column = COL_TENNANT;
	filter[0].value = tennant;
	filter[1].column = COL_INAME;
	filter[1].value = iname;
	filter[2].column = COL_STATE;
	filter[2].value = oldstate;

	return api->table_op(OP_UPDATE, TABLE_CONTAINERS, &slist, &flist, acfg);
}

static int old_change_container_state_batch(const char *tennant,
					const char *oldstate,
					const char *newstate,
					const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("UPDATE containers set state ='", newstate,
		       "' WHERE tennant = '", tennant,
		       "' AND state = '", oldstate,
		       "' AND hostname = '", acfg->cmdline.hostname,
		       "'");

	return api->send_raw_sql(sql, acfg);
}

int change_container_state_batch(const char *tennant,
					const char *oldstate,
					const char *newstate,
					const struct agent_config *acfg)
{
	struct colvallist slist;
	struct colvallist flist;
	struct colval set[1];
	struct colval filter[3];

	if (!api->table_op)
		return old_change_container_state_batch(tennant, oldstate, newstate, acfg);

	slist.count = 1;
	flist.count = 3;
	slist.entries = set;
	flist.entries = filter;

	set[0].column = COL_STATE;
	set[0].value = newstate;

	filter[0].column = COL_TENNANT;
	filter[0].value = tennant;
	filter[1].column = COL_HOSTNAME;
	filter[1].value = acfg->cmdline.hostname;
	filter[2].column = COL_STATE;
	filter[2].value = oldstate;

	return api->table_op(OP_UPDATE, TABLE_CONTAINERS, &slist, &flist, acfg);
}

int notify_host(const enum listen_channel chn, const char *host,
		const struct agent_config *acfg)
{
	char *name;

	if (!api->notify)
		return -EOPNOTSUPP;

	name = strjoina(channel_map[chn], "-", host, NULL);

	return api->notify(NOTIFY_HOST, chn, name, acfg);

} 

int notify_tennant(const enum listen_channel chn, const char *tennant,
		const struct agent_config *acfg)
{
	char *name;

	if (!api->notify)
		return -EOPNOTSUPP;

	name = strjoina(channel_map[chn], "-", tennant, NULL);

	return api->notify(NOTIFY_TENNANT, chn, name, acfg);
}

int notify_all(const enum listen_channel chn, const struct agent_config *acfg)
{
	char *name;

	if (!api->notify)
		return -EOPNOTSUPP;
	name = strjoina(channel_map[chn], "-all", NULL);

	return api->notify(NOTIFY_ALL, chn, name, acfg);
}

	
struct tbl* get_raw_table(enum db_table table, char *filter, const struct agent_config *acfg)
{
	if (!api->get_table)
		return NULL;

        return api->get_table(table, "*", filter, acfg);
}

int send_raw_sql(char *sql, const struct agent_config *acfg)
{
	if (!api->send_raw_sql)
		return -EOPNOTSUPP;
	return api->send_raw_sql(sql, acfg);
}

static int old_network_create_config(const char *name, const char *cfstring, const char *tennant, const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	/*
	 * until we implement network start/stop functions, we just mark
	 * all networks as active
	 */
	sql = strjoina("INSERT INTO networks VALUES('", name, "' , '"
			, tennant, "' , 'active', '", cfstring, "')", NULL);


	return api->send_raw_sql(sql, acfg);
	
}

int network_create_config(const char *name, const char *cfstring, const char *tennant, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[4];

	if (!api->table_op)
		return old_network_create_config(name, cfstring, tennant, acfg);

	list.count = 4;
	list.entries = values;

	values[0].column = COL_NAME;
	values[0].value = name;
	values[1].column = COL_TENNANT;
	values[1].value = tennant;
	values[2].column = COL_STATE;
	values[2].value = "active"; 
	values[3].column = COL_CONFIG;
	values[3].value = cfstring;

	return api->table_op(OP_INSERT, TABLE_NETWORKS, &list, NULL, acfg);
}

int network_create(const char *name, const char *configfile, const char *tennant, const struct agent_config *acfg)
{
	struct stat buf;
	char *configbuf;
	FILE *configp;
	int rc = -ENOENT;

	/*
	 * Start by reading in the config file to a string
	 */

	if (stat(configfile, &buf)) {
		/* Problem with the config file */
		return errno;
	}

	if (buf.st_size > 8192) {
		/* hm, suspciuosly large config */
		return -ERANGE;
	}

	configbuf = calloc(1, buf.st_size);

	if (!configbuf)
		return -ENOMEM;

	configp = fopen(configfile, "r");

	if (!configp)
		goto out_free;

	rc = ERANGE;
	if (fread(configbuf, buf.st_size, 1, configp) != 1)
		goto out_close;

	rc = network_create_config(name, configbuf, tennant, acfg);

out_close:
	fclose(configp);
out_free:
	free(configbuf);
	return rc;

}

int old_network_delete(const char *name, const char *tennant, const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM networks WHERE name='",name,"' AND tennant='",tennant,"'",NULL);

	return api->send_raw_sql(sql, acfg);
}

int network_delete(const char *name, const char *tennant, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[2];
	int rc;
	struct tbl *containers;
	char *filter = strjoina("tennant='",tennant,"' AND network='",name,"'", NULL);

	rc = 1;
	containers = get_raw_table(TABLE_NETMAP, filter, acfg);
	if (containers) {
		rc = containers->rows;
		free_tbl(containers);
	}

	if (rc) {
		LOG(ERROR, "Cannot delete a network with containers attached\n");
		return -EBUSY;
	}
	
	if (!api->table_op)
		return old_network_delete(name, tennant, acfg);

	
	list.count=2;
	list.entries = values;
	values[0].column = COL_NAME;
	values[0].value = name;
	values[1].column = COL_TENNANT;
	values[1].value = tennant;

	return api->table_op(OP_DELETE, TABLE_NETWORKS, NULL, &list, acfg);
}


int network_list(const char *tennant, const struct agent_config *acfg)
{
	struct tbl *networks;
	char *filter;
	int i;

	filter = strjoina("tennant='",tennant,"'",NULL);

	LOGRAW("Available networks\n");
	
	networks = get_raw_table(TABLE_NETWORKS, filter, acfg);

	if (!networks)
		return 0;

	LOGRAW("NETWORK NAME   |   STATE\n");
	LOGRAW("--------------------------------------------\n");
	for (i=0; i < networks->rows; i++) {
		LOGRAW("%-17s|%-8s\n",
			(char *)lookup_tbl(networks, i, COL_NAME),
			(char *)lookup_tbl(networks, i, COL_STATE));
	}


	free_tbl(networks);
	return 0;	
}

static int old_network_attach(const char *container, const char *network, const char *tennant, const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("INSERT INTO net_container_map VALUES('", tennant, "','",
		       container, "','", network, "')", NULL);

	return api->send_raw_sql(sql, acfg);
}

int network_attach(const char *container, const char *network, const char *tennant, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[3];

	if (!api->table_op)
		return old_network_attach(container, network, tennant, acfg);

	list.count = 3;
	list.entries = values;

	values[0].column = COL_TENNANT;
	values[0].value = tennant;
	values[1].column = COL_INAME;
	values[1].value = container;
	values[2].column = COL_CNAME;
	values[2].value = network;

	return api->table_op(OP_INSERT, TABLE_NETMAP, &list, NULL, acfg);
}

static int old_network_detach(const char *container, const char *network, const char *tennant, const struct agent_config *acfg)
{
	char *sql;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sql = strjoina("DELETE FROM net_container_map WHERE ",
		       "tennant='", tennant,"'",
		       "AND name='", container, "'",
		       "AND network='", network,"'", NULL);

	return api->send_raw_sql(sql, acfg);
}

int network_detach(const char *container, const char *network, const char *tennant, const struct agent_config *acfg)
{
	struct colvallist list;
	struct colval values[3];

	if (!api->table_op)
		return old_network_detach(container, network, tennant, acfg);

	list.count = 3;
	list.entries = values;

	values[0].column = COL_TENNANT;
	values[0].value = tennant;
	values[1].column = COL_INAME;
	values[1].value = container;
	values[2].column = COL_CNAME;
	values[2].value = network;

	return api->table_op(OP_DELETE, TABLE_NETMAP, NULL, &list, acfg);
}

static int auto_detach_networks_from_container(const char *iname, const char *tennant, const struct agent_config *acfg)
{
	struct tbl *networks;
	int i, rc;
	char *filter = strjoina("tennant='", tennant, "' AND name='", iname, "'", NULL);

	networks = get_raw_table(TABLE_NETMAP, filter, acfg);

	rc = 0;
	if (!networks)
		goto out;

	for (i = 0; i < networks->rows; i++) {
		rc = network_detach(iname, lookup_tbl(networks, i, COL_CNAME), tennant, acfg);
		if (rc)
			break;
	}

	free_tbl(networks);

out:
	return rc;
	
}

struct tbl * get_network_info(const char *network, const char *tennant, const struct agent_config *acfg)
{
	struct tbl *networks;
	char *filter;

	filter = strjoina("tennant='",tennant,"' AND name='", network, "'",NULL);

	
	networks = get_raw_table(TABLE_NETWORKS, filter, acfg);

	if (!networks)
		return 0;

	if (networks->rows == 0) {
		free_tbl(networks);
		networks = NULL;
	}

	return networks;
}


struct cfg_key_map {
	enum config_data_t type;
	size_t len;
	char *key_name;
};

struct cfg_key_map cfg_map[] = {
	[KEY_DB_VERSION] = { INT_TYPE, 4, "db_version" },
	[KEY_BASE_INTERVAL] = { INT_TYPE, 4, "base_interval"},
	[KEY_HEALTH_CHECK_MLT]   = { INT_TYPE, 4, "healthcheck_multiple"},
	[KEY_GC_MLT]       = { INT_TYPE, 4, "gc_multiple"}
};

static struct config_setting *_alloc_config_setting(enum config_data_k key)
{
	struct config_setting *new;

	new = calloc(sizeof(struct config_setting) + cfg_map[key].len, 1);

	if (!new)
		return NULL;

	new->key = key;
	new->type = cfg_map[key].type;
	new->len = cfg_map[key].len;

	switch(cfg_map[key].type) {
	case INT_TYPE:
		new->val.intval = (int *)new->extra_storage;
		break;
	default:
		LOG(ERROR, "Unknown data type\n");
		free(new);
		return NULL;
	}

	return new;	
}

struct config_setting *alloc_config_setting(char *key_name)
{
	int i;

	for(i=0; i < KEY_MAX; i++) {
		if (!strcmp(cfg_map[i].key_name, key_name))
			return _alloc_config_setting(i);
	}
	return NULL;
}

void free_config_setting(struct config_setting *cfg)
{
	free(cfg);
}

struct config_setting *get_global_config_setting(enum config_data_k key, const struct agent_config *acfg)
{
	struct config_setting *cfg = _alloc_config_setting(key);
	struct tbl *table;
	char *filter;

	filter = strjoina("key='", cfg_map[key].key_name, "'", NULL);

	table = api->get_table(TABLE_GCONF, "*", filter, acfg);

	if (!table->rows) {
		LOG(ERROR, "No config setting found for %s\n", cfg_map[key].key_name);
		goto out;
	}

	switch(cfg->type) {
	case INT_TYPE:
		*cfg->val.intval = strtol(lookup_tbl(table, 0, COL_CONFIG), NULL, 0);
		break;
	default:
		LOG(ERROR, "Unknown data type\n");
		free(cfg);
		return NULL;
	}

out:
	free_tbl(table);
	return cfg;
}

int set_global_config_setting(struct config_setting *set, const struct agent_config *acfg)
{
	char *sql;
	char *value;
	int rc;

	switch(set->type) {
	case INT_TYPE:
		asprintf(&value, "%d", *(int *)set->val.intval);
		break;
	default:
		return -EINVAL;
	}

	sql = strjoina("UPDATE global_config SET value='", value, "' WHERE key='", cfg_map[set->key].key_name, "'", NULL);
	free(value);

	rc = api->send_raw_sql(sql, acfg); 
	if (!rc)
		rc = notify_all(CHAN_GLOBAL_CONFIG, acfg);

	return rc;
}


struct tbl* get_global_config(const struct agent_config *acfg)
{
	return get_raw_table(TABLE_GCONF, NULL, acfg);
}

int update_node_metrics(const struct node_health_metrics *metrics, const struct agent_config *acfg)
{
	char *sql;
	char *loadstr;

	asprintf(&loadstr, "%d\n", metrics->load);

	sql = strjoina("UPDATE nodes SET load=", loadstr, " WHERE hostname='", acfg->cmdline.hostname,"'", NULL);
	free(loadstr);

	return api->send_raw_sql(sql, acfg);
}

char *alloc_db_v4addr(const char *netname, const char *tennant, const char *astart, const char *aend, const struct agent_config *acfg)
{
	struct tbl *available;
	char *sql;
	char *addr = NULL;
	struct in_addr start_addr;
	struct in_addr end_addr;
	char *filter = strjoin("name='", netname, "' AND tennant='", tennant, "' AND allocated='f'", NULL);

try_again:
	available = get_raw_table(TABLE_ALLOCMAP,  filter, acfg);
	free(filter);

	if (!available)
		goto find_new;

	if (available->rows == 0) {
		free_tbl(available);
		available = NULL;
		goto find_new;
	}

	/*
	 * Try to claim an unallocated address
	 */
	sql = strjoin("UPDATE net_address_allocation_map SET allocated='t' ",
		      "ownerhost='", acfg->cmdline.hostname, "' ",
		      "WHERE name='", netname, "' ",
		      "AND tennant='", tennant, "' ",
		      "AND address='", lookup_tbl(available, 0, COL_CONFIG), "'", NULL);

	send_raw_sql(sql, acfg);
	free(sql);

	/*
	 * Now we see if we got it
	 */
	filter = strjoin("name='", netname, "' AND tennant='", tennant, "' AND allocated='t'",
			 " AND address = '", lookup_tbl(available, 0, COL_CONFIG), "'",
			 " AND ownerhost='", acfg->cmdline.hostname, "'", NULL);
	free_tbl(available);
	available = get_raw_table(TABLE_ALLOCMAP, filter, acfg);
	free(filter);
	if (!available)
		goto find_new;
	if (available->rows == 0) {
		free_tbl(available);
		goto try_again;
	}

	/*
	 * We found 1 row in the table, so we got the address
	 */
	addr = strdup(lookup_tbl(available, 0, COL_CONFIG));
	free_tbl(available);
	goto out;		
	
	
find_new:
	/*
	 * We need to start hunting for an available address within our range
	 */
	filter = strjoin("name='", netname, "' AND tennant='", tennant, "' AND allocated='t'", NULL);
	available = get_raw_table(TABLE_ALLOCMAP, filter, acfg);
	free(filter);

	/*
	 * Given that we know from the prior bit that all addresses in the table are in use, we should start searching from the 
	 * end address, plus the number of addresses in the table
	 */
	inet_pton(AF_INET, astart, &start_addr);
	inet_pton(AF_INET, aend, &end_addr); 
	start_addr.s_addr += available->rows;
	free_tbl(available);

	while (start_addr.s_addr != end_addr.s_addr) {
		sql = strjoin("INSERT into net_address_allocation_map  VALUES (",
			      "'", netname, "', '", tennant, "','", inet_ntoa(start_addr),
			      "','t', null, '", acfg->cmdline.hostname, "')", NULL);
		send_raw_sql(sql, acfg);
		free(sql);
		filter = strjoin("name='", netname, "' AND tennant='", tennant, "' AND allocated='t'",
                         " AND address = '", inet_ntoa(start_addr), "'", NULL);
		available = get_raw_table(TABLE_ALLOCMAP, filter, acfg);
		free(filter);
		if (!available) {
			start_addr.s_addr++;
			continue;
		}
		if (available->rows == 0) {
			free_tbl(available);
			start_addr.s_addr++;
			continue;
		}
		/*
		 * Its our address!
		 */
		addr = strdup(inet_ntoa(start_addr));
		free_tbl(available);
		goto out;
	}
	
out:
	return addr;
}

char* alloc_db_v6addr(const char *netname, const char *tennant, const char *astart, const char *aend, const struct agent_config *acfg)
{
	return 0;
}


void release_db_v4addr(const char *netname, const char *tennant, const char *addr, const struct agent_config *acfg)
{
	char *sql = strjoina("UPDATE net_address_allocation_map set allocated='f' WHERE ",
			     "name='", netname, "' AND ",
			     "tennant='", tennant, "' address='", addr, "'", NULL);

	send_raw_sql(sql, acfg);
}

void release_db_v6addr(const char *netname, const char *tennant, const char *aadr, const struct agent_config *acfg)
{
	return;
}
