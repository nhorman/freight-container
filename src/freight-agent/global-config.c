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
 * *File: global-config.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description: Configur structures for freight-agent 
 * *********************************************************/

#include <stdlib.h>
#include <string.h>
#include <global-config.h>
#include <freight-db.h>
#include <freight-common.h>

int refresh_global_config(struct global_cfg *cfg, const struct agent_config *acfg)
{
	char *key, *value;
	int i, rc;
	struct tbl *config = get_global_config(acfg);

	rc = -ENOENT;
	if (!config)
		return -EFAULT;
	if (!config->rows)
		goto out;

	memset(cfg, 0, sizeof(struct global_cfg));
	for (i=0; i < config->rows; i++) {
		key = lookup_tbl(config, i, COL_NAME);
		value = lookup_tbl(config, i, COL_CONFIG);

		if (!strcmp(key, "db_version"))
			cfg->db_version = strtol(value, NULL, 10);
		else if (!strcmp(key, "base_interval"))
			cfg->base_interval = strtol(value, NULL, 10);
		else if (!strcmp(key, "healthcheck_multiple"))
			cfg->healthcheck_multiple = strtol(value, NULL, 10);
		else if (!strcmp(key, "gc_multiple"))
			cfg->gc_multiple = strtol(value, NULL, 10);
		else {
			LOG(ERROR, "Unexpected key %s\n", key);
			rc = -EINVAL;
			goto out;
		}
	}

	/*
	 * check here for version mismatches
	 */
	if (cfg->db_version != FREIGHT_DB_VERSION)
		rc = -E2BIG;

out:
	free_tbl(config);
	return rc;
}


