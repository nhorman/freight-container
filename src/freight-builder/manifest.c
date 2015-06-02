
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
#include <freight-common.h>
#include <manifest.h>

void release_repos(struct repository *repos) {
	struct repository *r = repos, *k = NULL;
	while (r) {
		k = r;
		free(r->name);
		free(k->url);
		r = r->next;
		free(k);
	}
}

void release_rpms(struct rpm *rpms) {
	struct rpm *r = rpms, *k = NULL;
	while (r) {
		k = r;
		free(r->name);
		r = r->next;
		free(k);
	}
}

CLEANUP_FUNC(struct repository*, release_repos);
CLEANUP_FUNC(struct rpm*, release_rpms);
#define __free_repos __cleanup(release_reposp)
#define __free_rpms __cleanup(release_rpmsp)

void release_manifest(struct manifest *manifest)
{
	if (!manifest)
		return;

	struct repository *repos = manifest->repos;
	struct rpm *rpms = manifest->rpms;
	void *killer;

	release_rpms(rpms);
/*
	while (rpms) {
		killer = rpms;
		rpms = rpms->next;
		free(killer);
	}
*/
	release_repos(repos);
/*
	while (repos) {
		killer = repos;
		free(repos->name);
		free(repos->url);
		repos = repos->next;
		free(killer);
	}
*/
	if (manifest->options)
		free(manifest->options);

	free(manifest->package.license);
	free(manifest->package.summary);
	free(manifest->package.release);
	free(manifest->package.version);
	free(manifest->package.name);
	free(manifest->package.post_script);
	free(manifest->yum.releasever);

	memset(manifest, 0, sizeof(struct manifest));
}

static int parse_yum_opts(struct config_t *config, struct manifest *manifest)
{
	if (!config || !manifest)
		return -EINVAL;

	config_setting_t *yum_opts = config_lookup(config, "yum_opts");
	config_setting_t *releasever;

	/*
 	 * yum opts aren't required
 	 */
	if (!yum_opts)
		return 0;

	releasever = config_setting_get_member(yum_opts, "releasever");
	if (releasever) {
		manifest->yum.releasever = strdup(
				config_setting_get_string(releasever));
		if (!manifest->yum.releasever) 
			return -ENOMEM;		
	}

	return 0;
}

static int parse_repositories(struct config_t *config, struct manifest *manifest)
{
	if (!config || !manifest)
		return -EINVAL;

	config_setting_t *repos = config_lookup(config, "repositories");
	config_setting_t *repo, *tmp;
	/*__free_repos */struct repository *repop = NULL;
	struct repository *last = manifest->repos;
	int i = 0;
	const char *name, *url;

	if (!repos)
		return 0;

	/*
 	 * Load up all the individual repository urls
 	 */
	while ((repo = config_setting_get_elem(repos, i)) != NULL) {

		tmp = config_setting_get_member(repo, "name");
		if (!tmp)
			return -EINVAL;
		name = config_setting_get_string(tmp);
		if (!name)
			return -EINVAL;
		tmp = config_setting_get_member(repo, "url");
		if (!tmp)
			return -EINVAL;
		url = config_setting_get_string(tmp);
		if (!url)
			return -EINVAL;

		repop = calloc(1, sizeof(struct repository));
		if (!repop)
			return -ENOMEM;

		repop->name = strdup(name);
		if (!repop->name)
			return -ENOMEM;

		repop->url = strdup(url);
		if (!repop->url) {
			free(repop->name);
			return -ENOMEM;
		}

		repop->next = NULL;
		if (last)
			last->next = repop;
		else
			manifest->repos = repop;
		last = repop;
		
		i++;
	}
	
	repop = NULL;

	return 0;
}

static int parse_rpms(struct config_t *config, struct manifest *manifest)
{
	config_setting_t *rpms = config_lookup(config, "manifest");
	config_setting_t *rpm_config;
	/*__free_rpms*/ struct rpm *rpmp = NULL;
	struct rpm *last = manifest->rpms;
	int i = 0;
	const char *name;

	if (!rpms)
		return 0;

	//printf("*-* *-*\n");
	/*
 	 * Load up all the individual repository urls
 	 */
	while ((rpm_config = config_setting_get_elem(rpms, i)) != NULL) {

		name = config_setting_get_string(rpm_config);
		if (!name)
			return -EINVAL;

		rpmp = calloc(1, sizeof(struct rpm));
		if (!rpmp)
			return -ENOMEM;

		rpmp->name = strdup(name);
		if (!rpmp->name)
			return -ENOMEM;

		//printf(" ** %s [%p %p %p]\n", rpmp->name, rpmp, last, rpmp->name);

		rpmp->next = NULL;
		if (last)
			last->next = rpmp;
		else
			manifest->rpms = rpmp;
		last = rpmp;
		
		i++;
	}

	//printf("*=* *=*\n");
	rpmp = NULL;

	return 0;
}

static int parse_packaging(struct config_t *config, struct manifest *manifest)
{
	config_setting_t *pkg = config_lookup(config, "packaging");
	config_setting_t *tmp;
	int rc = -EINVAL;

	if (!pkg) {
		LOG(ERROR, "You must supply a packaging directive\n");
		goto out;
	}

	rc = -ENOMEM;
	tmp = config_setting_get_member(pkg, "name");
	if (!tmp) {
		LOG(ERROR, "You must specify a package name\n");
		goto out;
	}
	manifest->package.name = strdup(config_setting_get_string(tmp));
	if (!manifest->package.name)
		goto out;

	tmp = config_setting_get_member(pkg, "version");
	if (!tmp) {
		LOG(ERROR, "You must specify a package version\n");
		goto out_name;
	}
	manifest->package.version = strdup(config_setting_get_string(tmp));
	if (!manifest->package.version)
		goto out_name;

	tmp = config_setting_get_member(pkg, "release");
	if (!tmp) {
		LOG(ERROR, "You must specify a package release\n");
		goto out_version;
	}
	manifest->package.release = strdup(config_setting_get_string(tmp));
	if (!manifest->package.release)
		goto out_version;

	tmp = config_setting_get_member(pkg, "summary");
	if (!tmp) {
		LOG(ERROR, "You must specify a package summary\n");
		goto out_release;
	}
	manifest->package.summary = strdup(config_setting_get_string(tmp));
	if (!manifest->package.summary)
		goto out_release;

	tmp = config_setting_get_member(pkg, "license");
	if (!tmp) {
		LOG(ERROR, "You must specify a package license\n");
		goto out_summary;
	}
	manifest->package.license = strdup(config_setting_get_string(tmp));
	if (!manifest->package.license)
		goto out_summary;

	tmp = config_setting_get_member(pkg, "author");
	if (!tmp) {
		LOG(ERROR, "You must specify a package author\n");
		goto out_license;
	}
	manifest->package.author = strdup(config_setting_get_string(tmp));
	if (!manifest->package.author)
		goto out_license;
	
	tmp = config_setting_get_member(pkg, "post_script");
	if (tmp) {
		manifest->package.post_script = 
			strdup(config_setting_get_string(tmp));
		if (!manifest->package.post_script)
			goto out_author;
	}

	rc = 0;
	goto out;

out_author:
	free(manifest->package.author);
out_license:
	free(manifest->package.license);
out_summary:
	free(manifest->package.summary);
out_release:
	free(manifest->package.release);
out_version:
	free(manifest->package.version);
out_name:
	free(manifest->package.name);
out:
	return rc;
}

static int parse_container_opts(config_t *config, struct manifest *manifest)
{
	config_setting_t *copts = config_lookup(config, "container_opts");
	config_setting_t *tmp;

	if (!copts)
		return -ENOENT;

	tmp = config_setting_get_member(copts, "user");

	if (tmp) {
		manifest->copts.user = strdup(
			config_setting_get_string(tmp));
		if (!manifest->copts.user)
			return -ENOMEM;
	}

	return 0;	
}

static int __read_manifest(const char *config_path, struct manifest *manifest,
			   int base)
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
		LOG(ERROR, "Error, manifest file %s does not exist\n",
			config_path);
		rc = -ENOENT;
		goto out;
	}

	if (config_read_file(&config, config_path) == CONFIG_FALSE) {
		LOG(ERROR, "Error in %s:%d : %s\n", 
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
		rc = __read_manifest(next_path, manifest, 0);
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

	if (!base) {
		if (config_lookup(&config, "packaging") != NULL) {
			LOG(ERROR, "Can't include packaging directive in "
				"inherited manifest %s\n", config_path);
			rc = -EINVAL;
			goto out;
		}
	} else {
		rc = parse_packaging(&config, manifest);
		if (rc)
			goto out;
	}

	/*
 	 * Don't need to bother with the return code
 	 * since this section is optional
 	 */
	if (base) {
		parse_yum_opts(&config, manifest);
		parse_container_opts(&config, manifest);
	}
out:
	config_destroy(&config);
	return rc;
}

int read_manifest(char *config_path, struct manifest *manifestp)
{
	int rc;

	memset(manifestp, 0, sizeof(struct manifest));

	rc = __read_manifest(config_path, manifestp, 1);

	if (rc) {
		release_manifest(manifestp);
		goto out;
	}

out:
	return rc;
}
