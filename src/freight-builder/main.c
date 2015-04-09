#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <manifest.h>
#include "config.h"

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"manifest", 1, NULL, 'm'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	fprintf(stderr, "%s [-h | --help] <[-m | --manifest]  config>\n", argv[0]);
#else
	frpintf(stderr, "%s [-h] <-m config>\n", argv[0]);
#endif
}

int main(int argc, char **argv)
{
	int opt, longind;
	struct manifest manifest;
	char *config = NULL;
	int rc = 1;

	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, "h,m:", lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, "h,m:") != -1) {
#endif
		switch(opt) {

		case '?':
			/* FALLTHROUGH */
		case 'h':
			usage(argv);
			goto out;
			/* NOTREACHED */
			break;
		case 'm':
			config = optarg;
			break;
		}
	}

	/*
 	 * Do some sanity checks
 	 */
	if (config == NULL) {
		fprintf(stderr, "Need to specify a manifest file\n");
		goto out;
	}

	/*
 	 * Parse the manifest file out
 	 */
	if (read_manifest(config, &manifest)) {
		goto out_release;
	}

	rc =0;
out_release:
	release_manifest(&manifest);
out:

	return rc;
}

