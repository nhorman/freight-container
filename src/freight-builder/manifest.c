#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <libconfig.h>
#include "manifest.h"

/*
 * This represents the chain of mainfest files that we read in
 */
struct config_chain {
	config_t config;
	struct config_chain *parent;
};


static struct config_chain *base_config = NULL;

static void close_config_files(struct config_chain *conf)
{
	struct config_chain *tmp, *index;

	index = conf;

	while (index) {
		tmp = index->parent;

		config_destroy(&index->config);

		free(index);

		index = tmp;
	}
}


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

static int __read_manifest(const char *config_path, struct config_chain *conf, struct manifest *manifest)
{
	int rc = 0;
	const char *next_path;
	config_t *config = &conf->config;

	/*
 	 * initalize the config structure
 	 */
	config_init(config);


	if (config_read_file(config, config_path) == CONFIG_FALSE) {
		fprintf(stderr, "Error in %s:%d : %s\n", 
			config_error_file(config), config_error_line(config),
			config_error_text(config));
		config_destroy(config);
		rc = -EINVAL;
		goto out;
	}

	/*
 	 * Good config file, look for an inherit directive
 	 */
	if (config_lookup_string(config, "inherit", &next_path) == CONFIG_TRUE) {
		/*
 		 * We have a file to inherit, so lets setup a new config
 		 * structure
 		 */
		conf->parent = calloc(1, sizeof(struct config_chain));
		if (!conf->parent) {
			config_destroy(config);
			rc = -ENOMEM;
			goto out;
		}

		/*
 		 * Recursively parse the parent manifest
 		 */
		rc = __read_manifest(next_path, conf->parent, manifest);
		if (rc) {
			free(conf->parent);
			config_destroy(config);
			goto out;
		}
	}


	/*
 	 * Now add in our manifest directives
 	 */
	rc = parse_repositories(config, manifest);
	if (rc)
		goto out;

	rc = parse_rpms(config, manifest);
	if (rc)
		goto out;


out:
	return rc;
}

int read_manifest(char *config_path, struct manifest *manifestp)
{
	int rc;

	base_config = calloc(1, sizeof(struct config_chain));
	if (!base_config)
		return -ENOMEM;

	memset(manifestp, 0, sizeof(struct manifest));

	rc = __read_manifest(config_path, base_config, manifestp);

	if (rc) {
		release_manifest(manifestp);
		free(base_config);
		goto out;
	}

out:
	close_config_files(base_config);
	return rc;
}
