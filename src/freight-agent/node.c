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
 * *File: node.c
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
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <dirent.h>
#include <fcntl.h>
#include <libconfig.h>
#include <node.h>
#include <freight-networks.h>
#include <freight-log.h>
#include <global-config.h>
#include <freight-common.h>

#define BTRFS_SUPER_MAGIC     0x9123683E

#define FORK_TABLE_WORKER 1

static struct global_cfg gcfg;

static int delete_all_tennants(const struct agent_config *acfg);

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

void clean_container_root(const struct agent_config *acfg)
{
	char *cmd;

	delete_all_tennants(acfg);

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
		char *cmd = strjoina("chroot ", acfg->node.container_root, "/", tenant,
				     " dnf local ", status, NULL); 

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

	yumcmd = strjoina("chroot ", troot, " dnf -y --nogpgcheck install ", rpm, NULL);
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

	yumcmd = strjoina("chroot ", troot, " dnf -y --nogpgcheck update ", rpm, NULL);
	rc = run_command(yumcmd, acfg->cmdline.verbose);
	return rc;

}

int uninstall_container(const char *rpm, const char *tenant,
			struct agent_config *acfg)
{
	char *yumcmd, *croot, *troot;
	struct stat buf;
	int rc = -ENOENT;

	troot = strjoina(acfg->node.container_root, "/", tenant, NULL);
	croot = strjoina(troot, "/containers/", rpm, NULL);

	if (stat(croot, &buf) == -ENOENT)
		goto out;

	yumcmd = strjoina("chroot ", troot, "dnf -y erase ", rpm, NULL);

	rc = run_command(yumcmd, acfg->cmdline.verbose);
out:
	return rc;
}

static int init_tennant(const char *croot,
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
	struct stat buf;
	struct tbl *yum_config = NULL;

	troot = strjoina(croot, "/", tenant, "/", NULL);

	rc = stat(troot, &buf);
	if (!rc) {
		/*
		 * This is a bit wierd because we expect the directory to not exist
		 */
		LOG(INFO, "Tennant %s already setup\n", tenant);
		return 0;
	}

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
			repo = strjoina(troot, "/etc/yum.repos.d/", lookup_tbl(yum_config, i, COL_NAME), ".repo", NULL);
			fptr = fopen(repo, "w");
			if (!fptr) {
				LOG(ERROR, "Unable to write /etc/yum.repos.d/%s.repo\n",
					(char *)lookup_tbl(yum_config, i, COL_NAME));
				goto out;
			}

			fprintf(fptr, "[%s]\n", (char *)lookup_tbl(yum_config, i, COL_NAME));
			fprintf(fptr, "name=%s\n", (char *)lookup_tbl(yum_config, i, COL_NAME));
			fprintf(fptr, "baseurl=%s\n", (char *)lookup_tbl(yum_config, i, COL_URL));
			fprintf(fptr, "gpgcheck=0\n"); /* for now */
			fprintf(fptr, "enabled=1\n");
			fclose(fptr);
		}

		free_tbl(yum_config);
	}

	/*
	 * Subscribe to the corresponding channel
	 */
	channel_add_tennant_subscription(acfg, CHAN_TENNANT_HOSTS, tenant);
	channel_add_tennant_subscription(acfg, CHAN_CONTAINERS, tenant);
out:
	return rc;
}

static int delete_tennant(const char *tennant, const struct agent_config *acfg)
{
	char *cmd;
	char *troot;
	int rc;

	troot = strjoina(acfg->node.container_root, tennant, NULL);
	cmd = strjoin("for i in `btrfs sub list ", acfg->node.container_root, 
		      "/ | awk ' /", tennant, "/ {print length, $0}' | sort -r -n |",
		      " awk '{print $10}'`; do ",
		      "btrfs sub del -c ", acfg->node.container_root,
		      "/$i; done", NULL);
	rc = run_command(cmd, acfg->cmdline.verbose);

	if (rc)
		LOG(ERROR, "Unable to delete tennant %s: %s\n",
			tennant, strerror(rc));
	free(cmd);
	cmd = strjoin("btrfs sub del -c ", troot, NULL);
	rc = run_command(cmd, acfg->cmdline.verbose);
	if (rc)
		LOG(ERROR, "Unable to delete tennant root for %s: %s\n",
			tennant, strerror(rc));

	/*
	 * Unsubscribe from the corresponding channel
	 */
	channel_del_tennant_subscription(acfg, CHAN_TENNANT_HOSTS, tennant);
	channel_del_tennant_subscription(acfg, CHAN_CONTAINERS, tennant);
	return rc; 
}

static int delete_all_tennants(const struct agent_config *acfg)
{
	DIR *tdir;
	struct dirent *entry;

	tdir = opendir(acfg->node.container_root);
	if (!tdir)
		return 0;

	while ((entry = readdir(tdir)) != NULL) {
		if (!strcmp(entry->d_name, "common"))
			continue;
		if (!strcmp(entry->d_name, "."))
			continue;
		if (!strcmp(entry->d_name, ".."))
			continue;
		delete_tennant(entry->d_name, acfg);
	}

	closedir(tdir);
	return 0;
}

int init_container_root(const struct agent_config *acfg)
{
	const char *croot = acfg->node.container_root;
	int rc = -EINVAL;
	struct tbl *table = NULL;
	char cbuf[1024];

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
	sprintf(cbuf, "dnf --installroot=%s/common "
		      "--nogpgcheck --releasever=21 -y "
		      "install dnf bash btrfs-progs\n", croot); 
	rc = run_command(cbuf, acfg->cmdline.verbose);
	if (rc) {
		LOG(ERROR, "Failed to install support utilities\n");
		goto out;
	}

	sprintf(cbuf, "dnf --installroot=%s/common "
		      "clean all\n", croot);
	run_command(cbuf, acfg->cmdline.verbose);

	free_tbl(table);
out:
	return rc;
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

static void delete_container_instance(const char *iname, const char *cname, const char *tennant, const struct agent_config *acfg)
{
	char *cmd;
	char *path;

	LOG(DEBUG, "Deleting container %s for tennant %s \n", iname, tennant);

	path = strjoina(acfg->node.container_root, tennant, "/containers/", cname, "/", NULL);
	
	cmd = strjoina("set -x; for i in `btrfs sub list ", path,
			" | awk ' /", iname, "/ {print length, $0}' | sort -r -n |",
			" awk '{print $10}'`\n",
			"do\n",
			"echo deleting $i\n",
			"btrfs sub del -c ", acfg->node.container_root,
			tennant, "/$i\n done", NULL);

	if (run_command(cmd, acfg->cmdline.verbose))
		LOG(ERROR, "Unable to delete container %s\n", iname);
	
}

int poweroff_container(const char *iname, const char *cname, const char *tennant,
		       const struct agent_config *acfg)
{
	pid_t pid;
	int eoc = 3; /* machinectl poweroff <iname> */
	char **execarray = NULL;
	char *machinecmd;
	int status;
	int rc;
	
	eoc++; /* NULL terminator */

	execarray = alloca(sizeof(const char *) * eoc);

	pid = fork();
	if (pid < 0)
		return errno;

	/*
	 * parent
	 */
	if (pid > 0) {

		/*
		 * Wait for the child to exit
		 */
		waitpid(pid, &status, 0);
		rc = WEXITSTATUS(status);

		/*
		 * we still have to wait until the machine is shutdown,
		 * as that is an async operation
		 */
		machinecmd = strjoin("machinectl status ", iname, " > /dev/null 2>&1 ", NULL);

		rc = 0;

		while (!rc) {
			rc = system(machinecmd);
		}	

		free(machinecmd);

		LOG(INFO, "container %s is shutdown\n", iname);
		rc = 0;

		/* 
		 * Then clean up the container snapshot
		 */
		delete_container_instance(iname, cname, tennant, acfg);

		return rc;
	}

	daemonize(acfg);

	eoc = 0;
	execarray[eoc++] = "machinectl"; /*argv[0]*/
	execarray[eoc++] = "poweroff";
	execarray[eoc++] = (char *)iname;
	execarray[eoc++] = NULL;

	exit(execvp("machinectl", execarray)); 	

	/* NOT REACHED */
	return 0;
}

int exec_container(const char *rpm, const char *name, const char *tenant,
                   const struct ifc_list *ifcs, int should_fork, const struct agent_config *acfg)
{
	pid_t pid;
	int i;
	int eoc;
	struct container_options copts;
	char *config_path;
	char *instance_path;
	char *btrfscmd;
	char** execarray = NULL;
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

	eoc += ifcs->count;

	if (copts.user)
		eoc += 2; /* -u <user> */

	eoc++; /* NULL teriminator */

	/*
 	 * Allocate the argv array
 	 */
	execarray = alloca(sizeof(const char *) * eoc);
	if (!execarray)
		exit(1);

	config_path = strjoina(acfg->node.container_root, "/", tenant, "/containers/", rpm, "/containerfs", NULL);
	instance_path = strjoina(acfg->node.container_root, "/", tenant, "/containers/", rpm, "/", name, NULL);

	btrfscmd = strjoina("btrfs subvolume snapshot ", config_path, " ", instance_path, NULL);
	run_command(btrfscmd, 0);

	/*
	 * After we create the snapshot, setup the networks in the container
	 */	
	setup_networks_in_container(rpm, name, tenant, ifcs, acfg);

	eoc = 0;
	execarray[eoc++] = "systemd-nspawn"; /* argv[0] */
	execarray[eoc++] = "-D"; /* -D */
	execarray[eoc++] = instance_path; /* <dir> */
	execarray[eoc++] = "-M"; /*-M*/
	execarray[eoc++] = (char *)name;
	execarray[eoc++] = "-b"; /* -b */
	if (copts.user) {
		execarray[eoc++] = "-u"; /* -u */
		execarray[eoc++] = copts.user; /* <user> */
	}

	for (i = 0; i < ifcs->count; i++)
		execarray[eoc++] = strjoina("--network-interface=", ifcs->ifc[i].container_veth);

	execarray[eoc++] = NULL;

	if (should_fork)
		exit(execvp("systemd-nspawn", execarray));
	else
		execvp("system-nspawn", execarray);

	/* NOTREACHED */
	return 0;
}

static int create_table_worker(const struct agent_config *config,
			       void (*work)(const void *data, const struct agent_config *),
			       const void *data, int *status)
{
#if FORK_TABLE_WORKER
	pid_t rc;
#endif
	struct agent_config *acfg = (struct agent_config *)config;
#if FORK_TABLE_WORKER
	rc = fork();

	if (rc < 0)
		return rc;

	if (rc) {
		/*
		 * We're the parent
		 */
		if (status) {
			waitpid(rc, status, 0);
			return WEXITSTATUS(*status);
		} else
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

	if (db_init(acfg)) {
		LOG(WARNING, "Could not re-init database connection\n");
		goto out;
	}

	if (db_connect(acfg)) {
		LOG(WARNING, "Could not establish new database connection\n");
		goto out;
	}
#endif
	work(data,acfg);
#if FORK_TABLE_WORKER 
	db_disconnect(acfg);
out:
#endif
	exit(0);
}

static enum event_rc handle_global_config_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{
	if (refresh_global_config(&gcfg, acfg)) {
		LOG(ERROR, "Unable to update global config\n");
		return EVENT_FAILED;
	}

	return EVENT_CONSUMED;
}

static enum event_rc handle_tennant_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{
	DIR *tdir;
	struct dirent *entry;
	int i, found;;

	struct tbl *tennants = get_tennants_for_host(acfg->cmdline.hostname, acfg);

	LOG(INFO, "Got a tennant host update\n");
	/*
	 * Handle leaving tennants first
	 */
	tdir = opendir(acfg->node.container_root);

	while ((entry = readdir(tdir)) != NULL)	{
		found = 0;
		if (!strcmp(entry->d_name, "common"))
			continue;
		if (!strcmp(entry->d_name, "."))
			continue;
		if (!strcmp(entry->d_name, ".."))
			continue;
		for (i=0; i < tennants->rows; i++) {
			if (!strcmp(entry->d_name, lookup_tbl(tennants, i, COL_TENNANT))) {
				found = 1;
				break;
			}
		}

		if (found)
			continue;
		/*
		 * We have a directory not in our tennant map, delete it
		 */
		delete_tennant(entry->d_name, acfg);
	}
	closedir(tdir);

	/*
	 * Now add new tennants
	 */
	for (i = 0; i < tennants->rows; i++) {
		init_tennant(acfg->node.container_root, lookup_tbl(tennants, i, COL_TENNANT), acfg);
	}
	free_tbl(tennants);	
	
	return EVENT_CONSUMED;
}

static void create_containers_from_table(const void *data, const struct agent_config *acfg)

{
	int i;
	int rc;
	char *tennant, *iname, *cname;
	const struct ifc_list *ifcs;
	struct tbl *containers = get_containers_for_host(acfg->cmdline.hostname, "start-requested", acfg);

	/*
	 * Do a batch update of the contanier states so we know they are all installing
	 * This also services to block the next table update from considering
	 * these as unserviced requests.
	 */
	if (change_container_state_batch(lookup_tbl(containers, 0, COL_TENNANT), "start-requested",
				         "installing", acfg)) {
		LOG(WARNING, "Unable to update container state\n");
		goto out;
	}

	for(i=0; i<containers->rows; i++) {
		tennant = lookup_tbl(containers, i, COL_TENNANT);
		iname = lookup_tbl(containers, i, COL_INAME);
		cname = lookup_tbl(containers, i, COL_CNAME);

		LOG(INFO, "Creating container %s of type %s for tennant %s\n",
			iname, cname, tennant);

		if (install_and_update_container(cname, tennant, acfg)) {
			LOG(WARNING, "Unable to install/update container %s\n", iname);
			change_container_state(tennant, iname,
					       "installing", "failed", acfg);
			continue;
		}

		ifcs = build_interface_list_for_container(iname, tennant, acfg);

		if (get_address_for_interfaces(ifcs, iname, acfg)) {
			LOG(ERROR, "Could not assign addresses for some interfaces\n");
			change_container_state(tennant, iname, "installing", "failed", acfg);
			continue;
		}

		create_and_bridge_interface_list(ifcs, acfg);

		/*
		 * They're installed, now we just need to exec each container
		 */
		LOG(INFO, "Booting container %s for tennant %s\n", iname, tennant);

		rc = exec_container(cname, iname, tennant, ifcs,
				    1, acfg);
		if (rc) {
			LOG(WARNING, "Failed to exec container %s: %s\n",
				iname, strerror(rc));
			change_container_state(tennant, iname, "installing", "failed", acfg);
			release_address_for_interfaces(ifcs, acfg);

		} else {
			LOG(INFO, "Started container %s\n", iname);
			change_container_state(tennant, iname, "installing", "running", acfg);
		}

		free_interface_list(ifcs);
        }

out:
	free_tbl(containers);
}

static void poweroff_containers_from_table(const void *data, const struct agent_config *acfg)
{
	int i;
	char *tennant, *iname, *cname;
	struct tbl *containers = get_containers_for_host(acfg->cmdline.hostname, "exiting", acfg);

	for(i=0; i<containers->rows; i++) {
		iname = lookup_tbl(containers, i, COL_INAME);
		tennant = lookup_tbl(containers, i, COL_TENNANT);
		cname = lookup_tbl(containers, i, COL_CNAME);

		LOG(INFO, "powering off container %s of type %s for tennant %s\n",
			iname, cname, tennant);
		if (poweroff_container(iname, cname, tennant, acfg)) {
			LOG(WARNING, "Could not poweroff container %s, failing operation\n", iname);
			change_container_state(tennant, iname, "exiting", "failed", acfg);
		} else
			change_container_state(tennant, iname, "exiting", "staged", acfg);

        }
	free_tbl(containers);
}

static void handle_new_containers(const struct agent_config *acfg)
{
	struct tbl *containers;
	int i;
	char *container;
	char *tennant;

	containers = get_containers_for_host(acfg->cmdline.hostname, "start-requested", acfg);

	if (!containers->rows) {
		LOG(DEBUG, "No new containers\n");
		goto out;
	}

	/*
	 * We need to do this here, before we have several threads trying to create the same network
	 */
	for (i=0; i< containers->rows; i++) {
		container = lookup_tbl(containers, i, COL_INAME);
		tennant = lookup_tbl(containers, i, COL_TENNANT);

		if (establish_networks_on_host(container, tennant, acfg)) {
			LOG(ERROR, "Unable to setup all networks for %s:%s\n",
				tennant, container);
			change_container_state(lookup_tbl(containers, i, COL_TENNANT),
					       lookup_tbl(containers, i, COL_INAME),
					       "start-requested", "failed", acfg);
		}
	}	


	if (create_table_worker(acfg, create_containers_from_table, NULL, NULL)) {
		LOG(WARNING, "Unable to fork container install process\n");
		change_container_state_batch(lookup_tbl(containers, 0, COL_TENNANT),
					     "installing", "failed", acfg);
	}

out:
	free_tbl(containers);
}

static void handle_exiting_containers(const struct agent_config *acfg)
{
	struct tbl *containers;
	int status;
	int i;

	containers = get_containers_for_host(acfg->cmdline.hostname, "exiting", acfg);

	LOG(INFO, "Handling Exiting Containers\n");

	if (!containers->rows) {
		LOG(DEBUG, "No exiting containers\n");
		goto out;
	}

	/*
	 * Note: Need to fork here so that each tennant can do installs in parallel
	 */
	if (create_table_worker(acfg, poweroff_containers_from_table, NULL, &status)) {
		LOG(WARNING, "Unable to fork container poweroff process\n");
		change_container_state_batch(lookup_tbl(containers, 0, COL_TENNANT),
					     "exiting", "failed", acfg);
		return;
	}

	/*
	 * because we use status above, we block until all containers are done powering off
	 * and we can safely cleanup host netowrks
	 */
	for (i = 0; i < containers->rows; i++)
		cleanup_networks_on_host(lookup_tbl(containers, i, COL_INAME),
					 lookup_tbl(containers, i, COL_TENNANT),
					acfg);

out:
	free_tbl(containers);

}
static enum event_rc handle_container_update(const enum listen_channel chnl, const char *extra,
					 const struct agent_config *acfg)
{


	handle_new_containers(acfg);

	handle_exiting_containers(acfg);

	/* We should handle failed containers assigned to us here */

	return EVENT_CONSUMED;
}


static bool request_shutdown = false;
static bool alarm_expired = false;

static void sigint_handler(int sig, siginfo_t *info, void *ptr)
{
	request_shutdown = true;
}

static void sigalrm_handler(int sig, siginfo_t *info, void *ptr)
{
	alarm_expired = true;
}

static void poweroff_all_containers(const struct agent_config *acfg)
{
	int i;
	struct tbl *containers;

	containers = get_containers_for_host(acfg->cmdline.hostname, "running", acfg);

	for (i = 0; i < containers->rows; i++) {
		poweroff_container(lookup_tbl(containers, i, COL_INAME),
				   lookup_tbl(containers, i, COL_CNAME),
				   lookup_tbl(containers, i, COL_TENNANT),
				   acfg);

		/*
		 * This function is called when the host is going down.
		 * We move the containers to failed state here because
		 * we want them rescheduled on another host
		 */
		change_container_state(lookup_tbl(containers, i, COL_TENNANT),
				      lookup_tbl(containers, i, COL_INAME),
				      "running", "failed", acfg);
	}	

	free_tbl(containers);
}

struct age_marker {
	const char *cname;
	const char *tennant;
	int gen;
	struct age_marker *next;
	struct age_marker *prev;
};

static struct age_marker *age_markers = NULL;

static struct age_marker* find_marker(const char *tennant, const char *cname)
{
	struct age_marker *idx = age_markers;

	for(idx = age_markers; idx != NULL; idx = idx->next) {
		if (!strcmp(tennant, idx->tennant) &&
		    !strcmp(cname, idx->cname))
			return idx;
	}
	return NULL;
}

static int container_has_children(const char *cname, const char *tennant,
				  const struct agent_config *acfg)
{
	char *troot;
	char *cmd;
	int rc;

	troot = strjoina(acfg->node.container_root, "/", tennant, NULL);
	cmd = strjoina("chroot ", troot, " rpm -q --whatrequires ", cname, NULL);

	rc = run_command(cmd, 0);
	/*
	 * an rc of 1 means rpm failed because the container had no children
	 */
	return !rc;	 
}

static void free_age_marker(struct age_marker *cage)
{
	if (cage->prev)
		cage->prev->next = cage->next;
	if (cage->next)
		cage->next->prev = cage->prev;
	free((void *)cage->cname);
	free((void *)cage->tennant);
	free(cage);
}


static void remove_tennant_unused_containers(const char *tennant,
					     const struct agent_config *acfg)
{
	char *cmd;
	char *troot;
	FILE *cmout;
	struct tbl *ctbl;
	char container[128];
	char *mark;
	struct age_marker *cage, *tmp;

	troot = strjoina(acfg->node.container_root, "/", tennant, NULL);

	/*
	 * Find all containers in the Containers/Freight group
	 */
	cmd = strjoin("chroot ", troot, " rpm -q --queryformat=\"%{NAME}\\n\" -g Containers/Freight", NULL);

	cmout = _run_command(cmd);
	while(fgets(container, 128, cmout)) {
		/*
		 * find any newlines
		 */
		mark = strchr(container, 10);
		if (mark)
			*mark = 0;
		/*
		 * Means we don't have any containers installed
		 */

		if (!strncmp(container, "group Containers/Freight", 24))
			break;
		ctbl = get_containers_of_type(container, tennant, acfg->cmdline.hostname, acfg);
		if (!ctbl->rows) {
			/*
			 * This contaienr rpm has no instances on this host
			 */
			cage = find_marker(tennant, container);
			if (cage) {
				cage->gen++;
				continue;
			}
			cage = calloc(1, sizeof(struct age_marker));
			cage->cname = strdup(container);
			cage->tennant = strdup(tennant);
			cage->next = age_markers;
			cage->prev = NULL;
			if (age_markers)
				age_markers->prev = cage;
			age_markers = cage;
		} else {
			/*
			 * We do have containers of this time, remove any pending age markers
			 */
			cage = find_marker(tennant, container);
			if (cage)
				free_age_marker(cage);
		}

		free_tbl(ctbl);
	}

	fclose(cmout);
	free(cmd);

	/*
	 * Now go through the list and look for expired containers
	 */
	for (cage = age_markers; cage != NULL; cage = cage->next) {
		if (strcmp(cage->tennant, tennant))
			continue;

		if (container_has_children(cage->cname, tennant, acfg))
			continue;

		if (cage->gen < gcfg.gc_multiple)
			continue;

		LOG(INFO, "Container %s in Tennant %s is being scrubbed\n", cage->cname, tennant);
		cmd = strjoin("chroot ", troot, " rpm -e ", cage->cname, NULL);
		run_command(cmd, acfg->cmdline.verbose);
		free(cmd);

		/*
		 * unlink and free the cage pointer
		 */
		tmp = cage;
		cage = cage->next;
		free_age_marker(tmp);
	}
}

static void clean_unused_containers(const struct agent_config *acfg)
{
	struct tbl *tennants;
	int r;

	tennants = get_tennants_for_host(acfg->cmdline.hostname, acfg);

	if (!tennants->rows)
		goto out;

	for (r = 0; r <  tennants->rows; r++) {
		remove_tennant_unused_containers(lookup_tbl(tennants, r, COL_TENNANT), acfg);
	}

out:
	free_tbl(tennants);
	return;
}

/*
 * Periodic function to update this nodes load in the database
 */
static void update_node_health(const struct agent_config *acfg)
{
	struct node_health_metrics health;
	float load;
	FILE *lfptr = fopen("/proc/loadavg", "r");

	if (!lfptr) {
		LOG(ERROR, "Can't open /proc/loadavg\n");
		return;
	}

	if (fscanf(lfptr, "%f", &load) != 1) {
		fclose(lfptr);
		LOG(ERROR, "Can't read /proc/loadavg\n");
		return;
	}

	health.load = round(load);	

	if(update_node_metrics(&health, acfg))
		LOG(WARNING, "Unable to update node health in db\n");
		
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
	struct sigaction alrmact;
	struct tbl *hostinfo;
	unsigned int health_count = 0;
	
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

	/*
	 * Put us in a known state regarding tennants
	 */
	LOG(INFO, "Cleaning tennant state\n");
	delete_all_tennants(config);

	if (rc == ENOENT) {
		LOG(ERROR, "please run freight-agent -m init first\n");
		goto out;
	}

	memset(&intact, 0, sizeof(struct sigaction));

	intact.sa_sigaction = sigint_handler;
	intact.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &intact, NULL);


	/*
	 * Wait for us to have an entry in the db
	 */
	while (request_shutdown == false) {
		LOG(INFO, "Waiting on our entry in the database\n");
		hostinfo = get_host_info(config->cmdline.hostname, config);
		if (hostinfo && hostinfo->rows == 1) {
			free_tbl(hostinfo);
			LOG(INFO, "Entry found, continuing\n");
			break;
		}
		free_tbl(hostinfo);
		sleep(5);
	}

	/*
	 * Join the node update channel
	 */
	if (channel_subscribe(config, CHAN_TENNANT_HOSTS, handle_tennant_update)) {
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

	/*
	 * Join the global config update channel
	 */
	if (channel_subscribe(config, CHAN_GLOBAL_CONFIG, handle_global_config_update)) {
		LOG(ERROR, "CAnnot subscribe to global config table\n");
		rc = -EINVAL;
		goto out_containers;
	}

	/*
	 * Mark ourselves as being operational
	 */
	change_host_state(config->cmdline.hostname, "operating", config);
	update_node_health(config);

	memset(&alrmact, 0, sizeof(struct sigaction));

	alrmact.sa_sigaction = sigalrm_handler;
	alrmact.sa_flags = SA_SIGINFO;
	sigaction(SIGALRM, &alrmact, NULL);
	alarm(gcfg.base_interval);
	rc = 0;

	while (request_shutdown == false) {
		wait_for_channel_notification(config);
		if (alarm_expired == true) {
			alarm_expired = false;
			health_count++;
			clean_unused_containers(config);
			if (health_count >= gcfg.healthcheck_multiple) {
				health_count = 0;
				update_node_health(config);
			}
			alarm(gcfg.base_interval);
		}
	}

	LOG(INFO, "Shutting down\n");
	alarm(0);
	poweroff_all_containers(config);

	change_host_state(config->cmdline.hostname, "offline", config);

	channel_unsubscribe(config, CHAN_GLOBAL_CONFIG);
out_containers:
	channel_unsubscribe(config, CHAN_CONTAINERS);
out_nodes:
	channel_unsubscribe(config, CHAN_TENNANT_HOSTS);
out:
	return rc;
}

