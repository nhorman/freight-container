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


/*
 * Global config data structures
 */
enum config_data_t {
	INT_TYPE
};

enum config_data_k {
	KEY_DB_VERSION = 0,
	KEY_BASE_INTERVAL,
	KEY_HEALTH_CHECK_MLT,
	KEY_GC_MLT,
	KEY_MAX
};

union config_data_u {
	int *intval;
};

struct config_setting {
	enum config_data_k key;
	enum config_data_t type;
	union config_data_u val;
	size_t len;
	char extra_storage[0];
};

struct node_health_metrics {
	unsigned int load;
};

/*
 * enum of table types in the db
 */
enum db_table {
	TABLE_TENNANTS = 0,
	TABLE_NODES,
	TABLE_TENNANT_HOSTS,
	TABLE_YUM_CONFIG,
	TABLE_CONTAINERS,
	TABLE_NETWORKS,
	TABLE_NETMAP,
	TABLE_EVENTS,
	TABLE_GCONF,
	TABLE_ALLOCMAP,
	TABLE_MAX
};

/*
 * enum of column names
 * Note that values above COL_MAX are special
 * purpose and do not address real db columns
 */
enum table_col {
	COL_TENNANT = 0,
	COL_HOSTNAME,
	COL_STATE,
	COL_NAME,
	COL_URL,
	COL_INAME,
	COL_CNAME,
	COL_PROXYPASS,
	COL_CONFIG,
	COL_PROXYADMIN,
	COL_LOAD,
	COL_MODIFIED,
	COL_MAX,
	COL_VERBATIM,
};

enum table_op { 
	OP_INSERT = 0,
	OP_UPDATE,
	OP_DELETE,
	OP_MAX
};

struct tbl {
	enum db_table type;
	int rows;
	int cols;
	char *** value;
};

enum listen_channel {
	CHAN_CONTAINERS = 0,
	CHAN_CONTAINERS_SCHED,
	CHAN_TENNANT_HOSTS,
	CHAN_GLOBAL_CONFIG,
	CHAN_NODES,
};

enum notify_type {
	NOTIFY_HOST = 0,
	NOTIFY_TENNANT,
	NOTIFY_ALL
};

enum event_rc {
	EVENT_CONSUMED = 0,
	EVENT_INTR,
	EVENT_NOCHAN,
	EVENT_FAILED,
};

struct colval {
	enum table_col column;
	const char *value;
};

struct colvallist {
	size_t count;
	struct colval *entries;
};

struct db_api {

	/* setup and teardown functions */
	int (*init)(struct agent_config *acfg);
	void (*cleanup)(struct agent_config *acfg);
	int (*connect)(struct agent_config *acfg);
	int (*disconnect)(struct agent_config *acfg);

	/* operational methods */
	int (*table_op)(enum table_op op, enum db_table tbl, const struct colvallist *setlist,
			const struct colvallist *filter,  const struct agent_config *acfg);
	int (*send_raw_sql)(const char *values, const struct agent_config *acfg);

	struct tbl* (*get_table)(enum db_table tbl, const char *cols, const char *filter,
				 const struct agent_config *acfg);

	int (*subscribe)(const char *lcmd, const char *chnl, const struct agent_config *acfg);

	int (*notify)(enum notify_type type, enum listen_channel chn,
		      const char *name, const struct agent_config *acfg);

	enum event_rc (*poll_notify)(const struct agent_config *acfg);
};

extern struct db_api postgres_db_api;
extern struct db_api sqlite_db_api;
extern struct db_api xmlrpc_api;
extern struct db_api *api;

static inline struct db_api* get_db_api(struct agent_config *acfg)
{

	switch (acfg->db.dbtype) {
	case DB_TYPE_POSTGRES:
		api = &postgres_db_api;
		return &postgres_db_api;
		break;
	case DB_TYPE_FREIGHTPROXY:
		api = &xmlrpc_api;
		return &xmlrpc_api;
		break;
	case DB_TYPE_SQLITE:
		api = &sqlite_db_api;
		return &sqlite_db_api;
		break;
	default:
		return NULL;
	}
}

static inline int db_init(struct agent_config *acfg)
{
	if (!api->init)
		return -EOPNOTSUPP;
	return api->init(acfg);	
}

static inline void db_cleanup(struct agent_config *acfg)
{
	if (!api->cleanup)
		return;
	return api->cleanup(acfg);	
}

static inline int db_connect(struct agent_config *acfg)
{
	if (!api->connect)
		return -EOPNOTSUPP;
	return api->connect(acfg);	
}

static inline int db_disconnect(struct agent_config *acfg)
{
	if (!api->disconnect)
		return -EOPNOTSUPP;
	return api->disconnect(acfg);	
}

static inline int wait_for_channel_notification(struct agent_config *acfg)
{
	if (!api->poll_notify)
		return -EOPNOTSUPP;
	return api->poll_notify(acfg);
}

extern int channel_subscribe(const struct agent_config *acfg,
			     const enum listen_channel chn,
			     enum event_rc (*hndl)(const enum listen_channel chnl, const char *data, const struct agent_config *acfg));

extern void channel_unsubscribe(const struct agent_config *acfg,
				const enum listen_channel chn);

int channel_add_tennant_subscription(const struct agent_config *acfg,
                                     const enum listen_channel chn,
                                     const char *tennant);

int channel_del_tennant_subscription(const struct agent_config *acfg,
                                     const enum listen_channel chn,
                                     const char *tennant);

extern enum event_rc event_dispatch(const char *chn, const char *extra);

extern const char* get_tablename(enum db_table id);
extern const enum db_table get_tableid(const char *name);

extern const char *get_colname(enum db_table tbl, enum table_col col);

extern struct tbl *alloc_tbl(int rows, int cols, enum db_table type);

extern void free_tbl(struct tbl *table);

extern int is_tbl_empty(struct tbl *table);

extern void *lookup_tbl(struct tbl *table, int row, enum table_col col);

extern char* get_tennant_proxy_pass(const char *user, const struct agent_config *acfg);

extern int get_tennant_proxy_admin(const char *user, const struct agent_config *acfg);

extern int add_repo(const char *name,
		    const char *url,
		    const char *tennant,
		    const struct agent_config *acfg);

extern int del_repo(const char *name,
		    const char *tennant,
		    const struct agent_config *acfg);

extern int add_host(const char *hostname,
		    const struct agent_config *acfg);

extern int del_host(const char *hostname,
		    const struct agent_config *acfg);

extern int subscribe_host(const char *host,
			  const char *tennant,
			  const struct agent_config *acfg);

extern int unsubscribe_host(const char *tennant,
			  const char *host,
			  const struct agent_config *acfg);

extern int change_host_state(const char *host, const char *newstate,
			     const struct agent_config *acfg);

extern int list_subscriptions(const char *tennant,
			     const struct agent_config *acfg);

extern struct tbl* get_tennants_for_host(const char *host,
				         const struct agent_config *acfg);

extern struct tbl* get_repos_for_tennant(const char *tennant,
					 const struct agent_config *acfg);

extern struct tbl* get_containers_for_host(const char *host, 
					   const char *state,
					   const struct agent_config *acfg);

extern struct tbl* get_containers_of_type(const char *cname,
						 const char *tennant,
						 const char *host,
						 const struct agent_config *acfg);

extern struct tbl *get_host_info(const char *name, const struct agent_config *acfg);

extern int assign_container_host(const char *name, const char *host,
                                 const char *tennant,
                                 const struct agent_config *acfg);

extern int request_create_container(const char *cname,
				    const char *iname,
				    const char *chost,
				    const char *tennant,
				    const struct agent_config *acfg);
extern int request_delete_container(const char *iname,
				    const char *tennant,
				    const int force,
				    const struct agent_config *acfg);

extern int request_boot_container(const char *iname,
				  const char *tennant,
				  const struct agent_config *acfg);

extern int request_poweroff_container(const char *iname,
				      const char *tennant,
				  const struct agent_config *acfg);

extern int print_container_list(const char *tennant, 
				const struct agent_config *acfg);

extern int change_container_state(const char *tennant,
				  const char *iname,
				  const char *oldstate,
				  const char *newstate,
				  const struct agent_config *acfg);

extern int change_container_state_batch(const char *tennant,
					const char *oldstate,
					const char *newstate,
					const struct agent_config *acfg);

extern int delete_container(const char *tennant,
			    const char *iname,
			    const struct agent_config *acfg);

extern int notify_host(const enum listen_channel chn, const char *host,
		       const struct agent_config *acfg);

extern int notify_tennant(const enum listen_channel chn, const char *tennant, 
			const struct agent_config *acfg);
extern int notify_all(const enum listen_channel chn, const struct agent_config *acfg);

extern struct tbl* get_raw_table(enum db_table table, char *filter, const struct agent_config *acfg);

extern int send_raw_sql(char *sql, const struct agent_config *acfg);

extern int network_create_config(const char *name, const char *cfstring, const char *tennant, const struct agent_config *acfg);

extern int network_create(const char *name, const char *configfile, const char *tennant, const struct agent_config *acfg);

extern int network_delete(const char *name, const char *tennant, const struct agent_config *acfg);

extern int network_list(const char *tennant, const struct agent_config *acfg);

extern int network_attach(const char *container, const char *network, const char *tennant, const struct agent_config *acfg);

extern int network_detach(const char *container, const char *network, const char *tennant, const struct agent_config *acfg);

extern struct tbl * get_network_info(const char *network, const char *tennant, const struct agent_config *acfg);

extern struct config_setting *alloc_config_setting(char *key_name);

extern void free_config_setting(struct config_setting *cfg);

extern struct config_setting *get_global_config_setting(enum config_data_k key, const struct agent_config *acfg);

extern int set_global_config_setting(struct config_setting *setting, const struct agent_config *acfg);

extern struct tbl* get_global_config(const struct agent_config *acfg); 

extern int update_node_metrics(const struct node_health_metrics *metrics, const struct agent_config *acfg);

extern char* alloc_db_v4addr(const char *netname, const char *tennant, const char *astart, const char *aend, const struct agent_config *acfg);
extern char* alloc_db_v6addr(const char *netname, const char *tennant, const char *astart, const char *aend, const struct agent_config *acfg);

extern void release_db_v4addr(const char *netname, const char *tennant, const char *addr, const struct agent_config *acfg);
extern void release_db_v6addr(const char *netname, const char *tennant, const char *aadr, const struct agent_config *acfg);
#endif
