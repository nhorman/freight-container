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


#include <stdio.h>
#include <errno.h>
#include <alloca.h>
#include <string.h>
#include <freight-log.h>
#include <freight-db.h>

int add_repo(const struct db_api *api,
	    struct yum_config *cfg,
	    const struct agent_config *acfg)
{
	char *sql = alloca(strlen(cfg->name)+strlen(cfg->url)+
			   strlen("INSERT into yum_config VALUES")+
			   128);

	if (!sql)
		return -ENOMEM;

	if (!api->send_raw_sql)
		return -EOPNOTSUPP;

	sprintf(sql, "INSERT into yum_config VALUES ('%s', '%s', '%s')",
		cfg->name, cfg->url, acfg->db.user);
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



static int print_subscription(const struct tbl_entry *entry)
{
	/*
 	 * We only print out column 0 here, as that holds the host name
 	 */
	if (!entry->col)
		LOGRAW("%s\n", entry->tbl_value);
	return 0;
}

int list_subscriptions(const struct db_api *api,
		       const char *tennant,
		       const struct agent_config *acfg)
{
	char filter[512];
	const char *real_tennant = tennant ? tennant : acfg->db.user;

	if (!api->show_table)
		return -EOPNOTSUPP;

	sprintf(filter, "tennant = '%s'", real_tennant);

	LOGRAW("\nHosts subscribed to tennant %s:\n", real_tennant);
	return api->show_table("tennant_hosts", "*", filter,
				print_subscription,
				acfg);
	
}
