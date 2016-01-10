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
 * *File: scheduler.c
 * *
 * *Author:Neil Horman
 * *
 * *Date: Jan 8, 2016
 * *
 * *Description: 
 * *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <dirent.h>
#include <fcntl.h>
#include <libconfig.h>
#include <scheduler.h>
#include <freight-networks.h>
#include <freight-log.h>
#include <global-config.h>
#include <freight-common.h>


static struct global_cfg gcfg;

static enum event_rc handle_global_config_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{
	if (refresh_global_config(&gcfg, acfg)) {
		LOG(ERROR, "Unable to update global config\n");
		return EVENT_FAILED;
	}

	return EVENT_CONSUMED;
}

static void select_host_for_container(const char *name, const char *tennant,
				      const struct agent_config *acfg)
{
	struct tbl *hosts;
	char *sql;
	int i, load, tmp, best;
	char *filter = strjoina("tennant='", tennant, "'", NULL);

	hosts = get_raw_table(TABLE_TENNANT_HOSTS, filter, acfg);

	if (!hosts) {
		LOG(WARNING, "Scheduler has no hosts to assign for container %s\n", name);
		return;
	}

	load=INT_MAX;
	best=0;

	if (!hosts->rows) {
		LOG(WARNING, "Scheduler has no hosts to assign for container %s\n", name);
		goto out;
	}

	for (i=0; i < hosts->rows; i++) {
		tmp = strtol(lookup_tbl(hosts, i, COL_LOAD), NULL, 10);
		if (tmp < load)
			best = i;
	}

	sql = strjoina("UPDATE nodes SET (state='staged', hostname='",
			lookup_tbl(hosts, best, COL_NAME), "')",
			"WHERE iname='",name,"' AND tennant='", tennant, "'", NULL);


	if (send_raw_sql(sql, acfg))
		LOG(ERROR, "Unable to update container assignment\n");
	
out:
	free_tbl(hosts);
	return;
}

static enum event_rc handle_container_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{
	struct tbl *containers;
	int i;

	containers = get_raw_table(TABLE_CONTAINERS, "state='assigning-host'", acfg);

	if (!containers)
		goto out;

	if (!containers->rows)
		goto out_free;

	for (i=0; i < containers->rows; i++)
		select_host_for_container(lookup_tbl(containers, i, COL_NAME),
					  lookup_tbl(containers, i, COL_TENNANT),
					  acfg);

out_free:
	free_tbl(containers);
out:
	return EVENT_CONSUMED;
}


static bool request_shutdown = false;

static void sigint_handler(int sig, siginfo_t *info, void *ptr)
{
	request_shutdown = true;
}

/*
 * This is our mode entry function, we setup freight-agent to act as a container
 * node here and listen for db events from this point
 */
int enter_scheduler_loop(struct agent_config *config)
{
	int rc = -EINVAL;
	struct sigaction intact;
	
	/*
	 * Join the container scheduler update channel
	 */
	if (channel_subscribe(config, CHAN_CONTAINERS_SCHED, handle_container_update)) {
		LOG(ERROR, "Cannot subscribe to database container updates\n");
		rc = EINVAL;
		goto out;
	}

	/*
	 * Join the global config update channel
	 */
	if (channel_subscribe(config, CHAN_GLOBAL_CONFIG, handle_global_config_update)) {
		LOG(ERROR, "CAnnot subscribe to global config table\n");
		rc = -EINVAL;
		goto out_containers;
	}

	memset(&intact, 0, sizeof(struct sigaction));

	intact.sa_sigaction = sigint_handler;
	intact.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &intact, NULL);
	
	rc = 0;

	while (request_shutdown == false)
		wait_for_channel_notification(config);

	LOG(INFO, "Shutting down\n");
	alarm(0);

	channel_unsubscribe(config, CHAN_GLOBAL_CONFIG);
out_containers:
	channel_unsubscribe(config, CHAN_CONTAINERS_SCHED);
out:
	return rc;
}

