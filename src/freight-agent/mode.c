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
 * *File: mode.c
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 21, 2015
 * *
 * *Description: 
 * *********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <libconfig.h>
#include <mode.h>
#include <freight-log.h>
#include <freight-common.h>

#define BTRFS_SUPER_MAGIC     0x9123683E

static char** execarray = NULL;


struct container_options {
	char *user;
};

static int parse_container_options(char *config_path,
			           struct container_options *copts)
{
	config_t config;
	config_setting_t *tmp, *tmp2;
	int rc = -EINVAL;

	memset(copts, 0, sizeof(struct container_options));

	config_init(&config);

	if(config_read_file(&config, config_path) == CONFIG_FALSE) {
		LOG(ERROR, "Cannot parse container config %s: %s\n",
		    config_path, strerror(errno));
		goto out;
	}

	tmp = config_lookup(&config, "container_opts");

	if (!tmp)
		goto out;

	rc = 0;
	tmp2 = config_setting_get_member(tmp, "user");
	if (!tmp2)
		goto out;

	copts->user = strdup(config_setting_get_string(tmp2));
	if (!copts->user)
		rc = -ENOMEM;

out:
	config_destroy(&config);
	return rc;
}

static int build_dir(const char *base, const char *path)
{
	char *p = strjoina(base, "/", path, NULL);
	return mkdir(p, 0700);
}

static char *build_path(const char *base, const char *path, char *name)
{
	return strjoin(base, path, name, NULL);
}

static void clean_tennant_root(const char *croot, 
			       const char *tennant,
			       const struct agent_config *acfg)
{
	char cmd[1024];

	sprintf(cmd, "btrfs subvolume delete %s/%s", croot, tennant);

	run_command(cmd, acfg->cmdline.verbose);
}

void clean_container_root(const struct agent_config *acfg)
{
	char hostname[512];
	char *cmd;
	struct tbl *table;
	int r;

	if (gethostname(hostname, 512)) {
		LOG(WARNING, "Could not get hostname: %s\n",
		    strerror(errno));
                goto clean_common;
        }

	table = get_tennants_for_host(hostname, acfg);
	if (!table) {
                LOG(WARNING, "Unable to obtain list of tennants\n");
                goto clean_common;
        }

	for(r=0; r < table->rows; r++)
                clean_tennant_root(acfg->node.container_root,
				   table->value[r][1], acfg);

	free_tbl(table);

clean_common:
	cmd = strjoina("btrfs subvolume delete ", acfg->node.container_root,
		       "/common");
	run_command(cmd, acfg->cmdline.verbose);

	cmd = strjoina("btrfs subvolume delete ", acfg->node.container_root);	

	run_command(cmd, acfg->cmdline.verbose);
	return;
}

void list_containers(char *scope, const char *tenant,
		     struct agent_config *acfg)
{
	if (streq(scope, "running"))
		run_command("machinectl list", 1);
	else {
		char *status = streq(scope, "local") ? "installed" : "all";
		char *cmd = strjoina("yum --installroot=", acfg->node.container_root, 
				"/", tenant, " local ", status, NULL);

		run_command(cmd, 1); 
	}
}

int install_container(const char *rpm, const char *tenant,
		      const struct agent_config *acfg)
{
	struct stat buf;
	int rc = -ENOENT;
	char *yumcmd;
	char *troot;

	troot = strjoina(acfg->node.container_root, "/", tenant, NULL);

	if (stat(troot, &buf) == -ENOENT) {
		LOG(ERROR, "Container root isn't initalized\n");
		goto out;
	}

	yumcmd = strjoina("yum --installroot=", troot, " -y --nogpgcheck install ", rpm, NULL);
	rc = run_command(yumcmd, acfg->cmdline.verbose);

out:
	return rc;
}

int install_and_update_container(const char *rpm, const char *tenant,
				 const struct agent_config *acfg)
{
	int rc;
	char *yumcmd;
	char *troot;

	rc = install_container(rpm, tenant, acfg);

	if (rc)
		return rc;


	troot = strjoina(acfg->node.container_root, "/", tenant, NULL);

	yumcmd = strjoina("yum --installroot=", troot, " -y --nogpgcheck update ", rpm, NULL);
	rc = run_command(yumcmd, acfg->cmdline.verbose);
	return rc;

}

int uninstall_container(const char *rpm, const char *tenant,
			struct agent_config *acfg)
{
	char *yumcmd, *croot;
	struct stat buf;
	int rc = -ENOENT;

	croot = strjoina(acfg->node.container_root, "/", tenant, "/containers/", rpm, NULL);

	if (stat(croot, &buf) == -ENOENT)
		goto out;

	yumcmd = strjoina("yum --installroot=", acfg->node.container_root, "/", tenant, " -y erase ", rpm, NULL);

	rc = run_command(yumcmd, acfg->cmdline.verbose);
out:
	return rc;
}

static int init_tennant_root(const char *croot,
			     const char *tenant,
			     const struct agent_config *acfg)
{
	char *dirs[]= {
		"containers",
		NULL,
	};
	char *troot;
	__free char *tmp = NULL;
	char *repo;
	int i, rc;
	FILE *fptr;
	struct tbl *yum_config = NULL;

	troot = strjoina(croot, "/", tenant, "/", NULL);

	/*
 	 * Sanity check the container root, it can't be the 
 	 * system root
 	 */
	rc = -EINVAL;
	LOG(INFO, "Building freight-agent environment\n");

	/*
	 * Start by creating a subvolume for the tennant
	 */
	repo = strjoina("btrfs subvolume snapshot ", croot,
			"/common ", troot);
	if (run_command(repo, acfg->cmdline.verbose)) {
		LOG(ERROR, "Unable to create a tennant subvolume\n");
		goto out;
	}

	LOG(INFO, "Building freight-agent environment\n");
	for (i=0; dirs[i] != NULL; i++) {
		rc = build_dir(troot, dirs[i]);
		if (rc) {
			LOG(ERROR, "Could not create %s: %s\n",
			   dirs[i], strerror(rc));
			goto out;
		}
	}

	/*
 	 * Create the yum.conf file
 	 * It should be able to rely on defaults as the environment is setup for
 	 * it
 	 */
	tmp = build_path(troot, "/etc/yum.conf", NULL);
	if (!tmp)
		return -ENOMEM;

	fptr = fopen(tmp, "w");
	if (!fptr) {
		rc = errno;
		LOG(ERROR, "Unable to write /etc/yum.conf: %s\n",
			strerror(errno));
		goto out;
	}

	fprintf(fptr, "[main]\n");
	fprintf(fptr, "cachedir=/var/cache/yum/$basearch/$releasever\n");
	fprintf(fptr, "logfile=/var/log/yum.log\n");
	fprintf(fptr, "gpgcheck=0\n"); /* ONLY FOR NOW! */
	fclose(fptr);

	sprintf(repo, "rm -f %s/etc/yum.repos.d/*", troot);
	if (run_command(repo, acfg->cmdline.verbose)) {
		LOG(ERROR, "Unable to remove cloned yum repos\n");
		goto out;
	}

	/*
 	 * Now we need to check the database for our repository configuration
 	 */
	yum_config = get_repos_for_tennant(tenant, acfg);
 
	if (!yum_config)
		LOG(WARNING, "No yum config in database, we won't be able "
			     "to fetch containers!\n");
	else {
		for (i=0; i < yum_config->rows; i++) {
			repo = strjoina(troot, "/etc/yum.repos.d/", yum_config->value[i][0], ".repo", NULL);
			fptr = fopen(repo, "w");
			if (!fptr) {
				LOG(ERROR, "Unable to write /etc/yum.repos.d/%s.repo\n",
					yum_config->value[i][0]);
				goto out;
			}

			fprintf(fptr, "[%s]\n", yum_config->value[i][0]);
			fprintf(fptr, "name=%s\n", yum_config->value[i][0]);
			fprintf(fptr, "baseurl=%s\n", yum_config->value[i][1]);
			fprintf(fptr, "gpgcheck=0\n"); /* for now */
			fprintf(fptr, "enabled=1\n");
			fclose(fptr);
		}

		free_tbl(yum_config);
	}
out:
	return rc;
}

int init_container_root(const struct agent_config *acfg)
{
	const char *croot = acfg->node.container_root;
	int rc = -EINVAL;
	struct tbl *table;
	char hostname[512];
	char cbuf[1024];
	int r;
	/*
	 * Sanity check the container root, it can't be the 
	 * system root
	 */
	rc = -EINVAL;
	if (streq(croot, "/")) {
		LOG(ERROR, "container root cannot be system root!\n");
		goto out;
	}

        /*
	 * Start by emptying the container root
	 */
        LOG(INFO, "Cleaning container root\n");
        clean_container_root(acfg);

	sprintf(cbuf, "btrfs subvolume create %s", croot);

	/*
 	 * Start by creating our support binanies for use with container
 	 * installs
 	 */
	rc = run_command(cbuf, acfg->cmdline.verbose);
	if (rc) {
		LOG(ERROR, "Failed to create container root subvolume\n");
		goto out;
	}

	sprintf(cbuf, "btrfs subvolume create %s/common", croot);

	rc = run_command(cbuf, acfg->cmdline.verbose);
	if (rc) {
		LOG(ERROR, "Failed to create container root common subvolume\n");
		goto out;
	}

	LOG(INFO, "Install support utilities.  This could take a minute..");
	sprintf(cbuf, "yum --installroot=%s/common "
		      "--nogpgcheck --releasever=21 -y "
		      "install sh btrfs-progs\n", croot); 
	rc = run_command(cbuf, acfg->cmdline.verbose);
	if (rc) {
		LOG(ERROR, "Failed to install support utilities\n");
		goto out;
	}

	sprintf(cbuf, "yum --installroot=%s/common "
		      "clean all\n", croot);
	run_command(cbuf, acfg->cmdline.verbose);

	/*
 	 * Now get a list of tennants
 	 */
	if (gethostname(hostname, 512)) {
		LOG(ERROR, "Could not get hostname: %s\n",
			strerror(errno));
		goto out;
	}

	table = get_tennants_for_host(hostname, acfg);
	if (!table) {
		LOG(ERROR, "Unable to obtain list of tennants\n");
		goto out;
	}

	/*
 	 * Now init the root space for each tennant
 	 */
	for(r = 0; r < table->rows; r++) {
		rc = init_tennant_root(croot, table->value[r][1], acfg);
		if (rc) {
			LOG(ERROR, "Unable to establish all tennants, cleaning...\n");
			goto out_clean;
		}
	}
out_free:
	free_tbl(table);
out:
	return rc;
out_clean:
	for(r=0; r < table->rows; r++)
		clean_tennant_root(croot, table->value[r][1], acfg);
	goto out_free;

}

static void daemonize(const struct agent_config *acfg)
{
	int i, fd;

	/* Become our own process group */
	setsid();

	/* pick a root dir until systemd fixes it up */
	chdir("/tmp");

	/*
 	 * Close all file descriptors
 	 */
	for ( i=getdtablesize(); i>=0; --i)   
		close(i);

	fd = open("/dev/null", O_RDWR, 0);
	if (fd != -1) {
		dup2 (fd, STDIN_FILENO);  
		dup2 (fd, STDOUT_FILENO);  
		dup2 (fd, STDERR_FILENO);  
    
		if (fd > 2)  
			close (fd);  
	}
}

int exec_container(const char *rpm, const char *name, const char *tenant,
                   int should_fork, const struct agent_config *acfg)
{
	pid_t pid;
	int eoc;
	struct container_options copts;
	char *config_path;

	config_path = strjoina(acfg->node.container_root, "/", tenant, "/containers/", rpm, "/container_config", NULL);

	/*
 	 * Lets parse the container configuration
 	 */
	if (parse_container_options(config_path, &copts))
		return -ENOENT;

	/*
 	 * Now we need to fork
 	 */
	if (should_fork) {
		pid = fork();

		/*
		 * Pid error
		 */
		if (pid < 0)
			return errno;	

		/*
		 * Parent should return immediately
		 */
		if (pid > 0)
			return 0;

		/*
		 * child from here out
		 * we should daemonize
		 * NOTE: AFTER DAEMONIZING, LOG() doesn't work, 
		 * we will need to use machinectl to check on status
		 */
		daemonize(acfg);
	}

	/*
 	 * Now lets start building our execv line
 	 */
	eoc = 6; /*systemd-nspawn -D <dir> -b -M <name> */

	if (copts.user)
		eoc += 2; /* -u <user> */

	eoc++; /* NULL teriminator */

	/*
 	 * Allocate the argv array
 	 */
	execarray = malloc(sizeof(const char *) * eoc);
	if (!execarray)
		exit(1);

	config_path = strjoina(acfg->node.container_root, "/", tenant, "/containers/", rpm, "/containerfs", NULL);

	eoc = 0;
	execarray[eoc++] = "systemd-nspawn"; /* argv[0] */
	execarray[eoc++] = "-D"; /* -D */
	execarray[eoc++] = config_path; /* <dir> */
	execarray[eoc++] = "-M"; /*-M*/
	execarray[eoc++] = (char *)name;
	execarray[eoc++] = "-b"; /* -b */
	if (copts.user) {
		execarray[eoc++] = "-u"; /* -u */
		execarray[eoc++] = copts.user; /* <user> */
	}
	execarray[eoc++] = NULL;

	exit(execvp("systemd-nspawn", execarray));
}

static int create_table_worker(const struct tbl *table, const struct agent_config *acfg,
			       void (*work)(const struct tbl *, const struct agent_config *))
{
	pid_t rc;

	rc = fork();

	if (rc < 0)
		return rc;

	if (rc) {
		/*
		 * We're the parent
		 */
		return 0;
	}

	/*
	 * We're the child
	 */

	/*
	 * Set our process group id to that of our parent
	 * This allows the parent to signal us with kill 
	 * as a group
	 */
	if (setpgid(getpid(), getpgid(getppid()))) {
		LOG(WARNING, "Could not set install worker pgid\n");
		exit(1);
	}

	work(table, acfg);
	exit(0);
}

static enum event_rc handle_node_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{
	return EVENT_CONSUMED;
}

static void create_containers_from_table(const struct tbl *containers, const struct agent_config *acfg)
{
	int i;
	for(i=0; i<containers->rows; i++) {
		LOG(INFO, "Creatig container %s of type %s for tennant %s\n",
			containers->value[i][1], containers->value[i][2], containers->value[i][0]);

		if (install_and_update_container(containers->value[i][2], containers->value[i][0], acfg)) {
			LOG(WARNING, "Unable to install/update container %s\n", containers->value[i][1]);
			change_container_state(containers->value[i][0], containers->value[i][1],
					       "failed", acfg);
			continue;
		}

		/*
		 * They're installed, now we just need to exec each container
		 */
		
        }
}

static enum event_rc handle_container_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{
	struct tbl *containers;

	LOG(DEBUG, "GOT A CHANNEL EVENT\n");

	containers = get_containers_for_host(acfg->cmdline.hostname, "new", acfg);

	if (!containers->rows) {
		LOG(DEBUG, "No new containers\n");
		goto out_free;
	}

	/*
	 * Do a batch update of the contanier states so we know they are all installing
	 * This also services to block the next table update from considering
	 * these as unserviced requests.
	 */
	if (change_container_state_batch(containers->value[0][0], "new",
				         "installing", acfg)) {
		LOG(WARNING, "Unable to update container state\n");
		goto out_err;
	}

	/*
	 * Note: Need to fork here so that each tennant can do installs in parallel
	 */
	if (create_table_worker(containers, acfg, create_containers_from_table)) {
		LOG(WARNING, "Unable to fork container install process\n");
		goto out_err;
	}

out:
	return EVENT_CONSUMED;

out_err:
	change_container_state_batch(containers->value[0][0], "installing", "failed", acfg);
out_free:
	free_tbl(containers);
	goto out;
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
int enter_mode_loop(struct agent_config *config)
{
	int rc = -EINVAL;
	struct stat buf;
	struct sigaction intact;
	
	/*
 	 * Start by setting up a clean container root
 	 * If its not already there
 	 */
	if (stat(config->node.container_root, &buf) != 0) {
		rc = errno; 
		if (rc != ENOENT) {
			LOG(ERROR, "Container root isn't available: %s\n",
				strerror(rc));
			LOG(ERROR, "Run freight-agent -m clean\n");
			goto out;
		}
	}

	if (rc == ENOENT) {
		LOG(ERROR, "please run freight-agent -m init first\n");
		goto out;
	}

	/*
	 * Join the node update channel
	 */
	if (channel_subscribe(config, CHAN_NODES, handle_node_update)) {
		LOG(ERROR, "Connot subscribe to database node updates\n");
		rc = EINVAL;
		goto out;
	}

	/*
	 * Join the container update channel
	 */
	if (channel_subscribe(config, CHAN_CONTAINERS, handle_container_update)) {
		LOG(ERROR, "Cannot subscribe to database container updates\n");
		rc = EINVAL;
		goto out_nodes;
	}

	memset(&intact, 0, sizeof(struct sigaction));

	intact.sa_sigaction = sigint_handler;
	intact.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &intact, NULL);
	
	rc = 0;

	/*
	 * Mark ourselves as being present and ready to accept requests
	 */
	change_host_state(config->cmdline.hostname, "operating", config);

	while (request_shutdown == false) {
		wait_for_channel_notification(config);
	}

	LOG(INFO, "Shutting down\n");
	change_host_state(config->cmdline.hostname, "offline", config);

	channel_unsubscribe(config, CHAN_CONTAINERS);
out_nodes:
	channel_unsubscribe(config, CHAN_NODES);
out:
	return rc;
}

