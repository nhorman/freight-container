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
#include <freight-log.h>
#include <freight-db.h>

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

int add_repo(const struct db_api *api,
	     const char *name, const char *url,
	    const struct agent_config *acfg)
{
	char *sql = alloca(strlen(name)+strlen(url)+
			   strlen("INSERT into yum_config VALUES")+
			   128);

	if (!sql)
		return -ENOMEM;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "INSERT into yum_config VALUES ('%s', '%s', '%s')",
		name, url, acfg->db.user);
	return api->send_raw_sql(sql, acfg);
}

extern int del_repo(const struct db_api *api,
		    const char *name,
		    const struct agent_config *acfg)
{
	char *sql = alloca(strlen(name) + strlen(acfg->db.user) +
			   strlen("DELETE from yum_config WHERE "
				  "name = AND tennant = ''''"));

	if (!sql)
		return -ENOMEM;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "DELETE from yum_config WHERE name = '%s' "
		     "AND tennant='%s'", name, acfg->db.user);

	return api->send_raw_sql(sql, acfg);
}


int add_host(const struct db_api *api,
	     const char *hostname,
	     const struct agent_config *acfg)
{
	char *sql = alloca(strlen(hostname)+
			   strlen("INSERT into nodes VALUES")+
			   128);

	if (!sql)
		return -ENOMEM;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "INSERT into nodes VALUES ('%s', '%s')",
		hostname, "offline");
	return api->send_raw_sql(sql, acfg);
}

int del_host(const struct db_api *api,
		    const char *hostname,
		    const struct agent_config *acfg)
{
	char *sql = alloca(strlen(hostname)+
			   strlen("DELETE from nodes WHERE hostname = ''"));

	if (!sql)
		return -ENOMEM;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "DELETE from nodes WHERE hostname = '%s'", hostname);

	return api->send_raw_sql(sql, acfg);
}

int subscribe_host(const struct db_api *api,
                          const char *tennant,
                          const char *host,
			  const struct agent_config *acfg)
{

	char *sql = alloca(strlen(tennant)+ strlen(host) + 
			   strlen("INSERT into tennant_hosts VALUES") + 128);

	if (!sql)
		return -ENOMEM;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "INSERT into tennant_hosts VALUES('%s', '%s')",
		tennant, host);

	return api->send_raw_sql(sql, acfg);
}

int unsubscribe_host(const struct db_api *api,
                          const char *tennant,
                          const char *host,
			  const struct agent_config *acfg)
{
	char sql[1024];

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "DELETE from tennant_hosts WHERE hostname ='%s' AND tennant = '%s'", host, tennant);

	return api->send_raw_sql(sql, acfg);

}


int list_subscriptions(const struct db_api *api,
		       const char *tennant,
		       const struct agent_config *acfg)
{
	char filter[512];
	struct tbl *table;
	int r;
	const char *real_tennant = tennant ? tennant : acfg->db.user;

	if (!api->get_table)
		return -EOPNOTSUPP;

	sprintf(filter, "tennant = '%s'", real_tennant);

	table = api->get_table("tennant_hosts", "*", filter, acfg);
	if (!table)
		LOGRAW("\nNo hosts subscribed to tennant %s\n", real_tennant);
	else {
		LOGRAW("\nHosts subscribed to tennant %s:\n", real_tennant);
		for (r = 0; r < table->rows; r ++)	
			LOGRAW("\t%s\n", table->value[r][0]);
	}
	free_tbl(table);
	return 0;
	
}

struct tbl* get_tennants_for_host(const struct db_api *api,
				   const char *host,
			  	   const struct agent_config *acfg)
{
	char filter[1024];

	if (!api->get_table)
		return NULL;

	sprintf(filter, "hostname = '%s'", host);

	return api->get_table("tennant_hosts", "*", filter, acfg);
}

struct tbl* get_repos_for_tennant(const struct db_api *api,
				  const char *tennant,
				  const struct agent_config *acfg)
{
	char filter[512];

	if (!api->get_table)
		return NULL;

	sprintf(filter, "tennant='%s'", tennant);

	return api->get_table("yum_config", "name, url", filter, acfg);
}
