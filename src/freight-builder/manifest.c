
/*********************************************************
 *Copyright (C) 2015 Neil Horman
 *This program is free software; you can redistribute it and\or modify
 *it under the terms of the GNU General Public License as published 
 *by the Free Software Foundation; either version 2 of the License,
 *or  any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *GNU General Public License for more details.
 *
 *File: manifest.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description
 * *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libconfig.h>
#include "manifest.h"

void release_manifest(struct manifest *manifest)
{
	struct repository *repos = manifest->repos;
	struct rpm *rpms = manifest->rpms;
	void *killer;

	while (rpms) {
		killer = rpms;
		rpms = rpms->next;
		free(killer);
	}

	while (repos) {
		killer = repos;
		repos = repos->next;
		free(killer);
	}

	if (manifest->options)
		free(manifest->options);

}

static int parse_repositories(struct config_t *config, struct manifest *manifest)
{
	config_setting_t *repos = config_lookup(config, "repositories");
	config_setting_t *repo;
	struct repository *repop;
	int i = 0;
	size_t alloc_size;
	const char *name, *url;

	if (!repos)
		return 0;

	/*
 	 * Load up all the individual repository urls
 	 */
	while ((repo = config_setting_get_elem(repos, i)) != NULL) {

		if (config_setting_lookup_string(repo, "name", &name) == CONFIG_FALSE)
			return -EINVAL;
		if (config_setting_lookup_string(repo, "url", &url) == CONFIG_FALSE)
			return -EINVAL;

		alloc_size = strlen(name) + strlen(url);

		repop = calloc(1, sizeof(struct repository) + alloc_size);
		if (!repop)
			return -ENOMEM;

		/*
 		 * Allocate memory for strings at the end of the repository
 		 * structure so that we can free it as a unit
 		 */
		repop->name = (char *)((void *)repop + sizeof(struct repository));
		repop->url = (char *)((void *)repop + sizeof(struct repository) + strlen(name))+1;
		strcpy(repop->name, name);
		strcpy(repop->url, url); 

		repop->next = manifest->repos;
		manifest->repos = repop;	
		
		i++;
	}

	return 0;
}

static int parse_rpms(struct config_t *config, struct manifest *manifest)
{
	config_setting_t *rpms = config_lookup(config, "manifest");
	config_setting_t *rpm_config;
	struct rpm *rpmp;
	int i = 0;
	size_t alloc_size;
	const char *name;

	if (!rpms)
		return 0;

	/*
 	 * Load up all the individual repository urls
 	 */
	while ((rpm_config = config_setting_get_elem(rpms, i)) != NULL) {

		name = config_setting_get_string(rpm_config);
		if (!name)
			return -EINVAL;

		alloc_size = strlen(name);

		rpmp = calloc(1, sizeof(struct rpm) + alloc_size);
		if (!rpmp)
			return -ENOMEM;

		/*
 		 * Allocate memory for strings at the end of the repository
 		 * structure so that we can free it as a unit
 		 */
		rpmp->name = (char *)(rpmp + 1);
		strcpy(rpmp->name, name);

		rpmp->next = manifest->rpms;
		manifest->rpms = rpmp;	
		
		i++;
	}

	return 0;
}

static int __read_manifest(const char *config_path, struct manifest *manifest)
{
	int rc = 0;
	const char *next_path;
	config_t config;
	struct stat buf;

	/*
 	 * initalize the config structure
 	 */
	config_init(&config);

	/*
 	 * Check for file existance
 	 */
	if (stat(config_path, &buf)) {
		fprintf(stderr, "Error, manifest file %s does not exist\n",
			config_path);
		rc = -ENOENT;
		goto out;
	}

	if (config_read_file(&config, config_path) == CONFIG_FALSE) {
		fprintf(stderr, "Error in %s:%d : %s\n", 
			config_error_file(&config), config_error_line(&config),
			config_error_text(&config));
		rc = -EINVAL;
		goto out;
	}

	/*
 	 * Good config file, look for an inherit directive
 	 */
	if (config_lookup_string(&config, "inherit", &next_path) == CONFIG_TRUE) {
		/*
 		 * Recursively parse the parent manifest
 		 */
		rc = __read_manifest(next_path, manifest);
		if (rc) 
			goto out;
	}


	/*
 	 * Now add in our manifest directives
 	 */
	rc = parse_repositories(&config, manifest);
	if (rc)
		goto out;

	rc = parse_rpms(&config, manifest);
	if (rc)
		goto out;


out:
	config_destroy(&config);
	return rc;
}

int read_manifest(char *config_path, struct manifest *manifestp)
{
	int rc;

	memset(manifestp, 0, sizeof(struct manifest));

	rc = __read_manifest(config_path, manifestp);

	if (rc) {
		release_manifest(manifestp);
		goto out;
	}

out:
	return rc;
}
