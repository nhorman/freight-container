#ifndef _MANIFEST_H_
#define _MAINFEST_H_



/*
 * Child elements for the manifest structure below
 */
struct repository {
	char *name;
	char *url;
	struct repository *next;
};

struct rpm {
	char *name;
	struct rpm *next;
};

struct options {
	char *nspawn_opts;
};

/*
 * This represents the set of parsed information we get out of the above
 * manifest files
 */
struct manifest {
	struct repository *repos;
	struct rpm *rpms;
	struct options *options;
};

void free_manifest(struct manifest *manifest);

int read_manifest(char *config_path, struct manifest **manifest);



#endif
