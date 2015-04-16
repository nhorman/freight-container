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
 *File: main.c
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description
 *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <config.h>
#include <freight-log.h>

/*
 * Global flag to tell us if we're daemonized
 */
int daemonize = 0;
struct agent_config config;

#ifdef HAVE_GETOPT_LONG
struct option lopts[] = {
	{"help", 0, NULL, 'h'},
	{"config", 1, NULL, 'c'},
	{"daemon", 0, NULL, 'd'},
	{ 0, 0, 0, 0}
};
#endif

static void usage(char **argv)
{
#ifdef HAVE_GETOPT_LONG
	fprintf(stderr, "%s [-h | --help] [-c | --config=<config>] [-d | --daemon]\n", argv[0]);
#else
	frpintf(stderr, "%s [-h] [-c <config>] [-d]", argv[0]);
#endif
}

int main(int argc, char **argv)
{
	int rc = 1;
	int opt, longind;
	char *config_file = "/etc/freight-agent/config";

	/*
 	 * Parse command line options
 	 */

#ifdef HAVE_GETOPT_LONG
	while ((opt = getopt_long(argc, argv, "hc:d", lopts, &longind)) != -1) {
#else
	while ((opt = getopt(argc, argv, "hc:d") != -1) {
#endif
		switch(opt) {

		case '?':
			/* FALLTHROUGH */
		case 'h':
			usage(argv);
			goto out;
			/* NOTREACHED */
			break;
		case 'c':
			config_file = optarg;
			break;
		case 'd':
			daemonize = 1;
			break;
		}
	}


	/*
 	 * Read in the configuration file
 	 */
	rc = read_configuration(config_file, &config);
	rc = 0;
out:
	return rc;
}

