#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libconfig.h>


struct config_chain {
	config_t config;
	struct config_chain *parent;
};

static struct config_chain *base_config = NULL;


int __read_manifest(const char *config_path, struct config_chain *conf)
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
		rc = __read_manifest(next_path, conf->parent);
		if (rc) {
			free(conf->parent);
			config_destroy(config);
			goto out;
		}
	}


	/*
 	 * Now add in our manifest directives
 	 */
	/* NH TBD */

out:
	return rc;
}

int read_manifest(char *config_path)
{
	int rc;
	base_config = calloc(1, sizeof(struct config_chain));
	if (!base_config)
		return -ENOMEM;

	rc = __read_manifest(config_path, base_config);

	if (rc)
		free(base_config);

	return rc;
}
