# Your First Freight Environment: A Tutorial

## Introduction
This document is meant to walk you through a local setup of freight so that you
can deploy and manage your own containers.

# STEP 1 : Build your containers
To start, we need to build some containers.  To do this we will need:
* A btrfs file system
* One or more manifest files
* The freight-builder utility

For this example we will use the 3 example manifests provided by freight.  They are:
* base - The base container which provides core systemd functionality
* httpd_container - extends base by adding a web server 
* my_website - customizes httpd_container by setting a root password and custom web page

These three containers inherit from each other in the order above, and so must
be built in that order, starting with base.  To build base run the following
command:

`freight-builder -w /btrfs/path -o /output/dir -m base.manifest`

This will produce the output rpm 
/output/dir/base-<version>-<release>.arch.rpm

Note, of course you should replace the path arguments with paths that are
appropriate for the system you are working on.

Next, build the httpd_container with the following command:
`freight-builder -w /btrfs/path -o /output/dir -p /output/dir -m httpd_container.manifest`


Note the addition of the -p option, which tells freight-builder to install any
rpms in the specified directory prior to building the new container.  This
allows freight-builder to properly generate the binary differential from the
parent container (base) to the new child container (httpd_container).

After completing this step you will have a new rpm in /output/dir representing
the httpd_container contiainer.

Last build the my_webiste container, repeast the last command, substituting
my_website.manifest for httpd_container.manifest

Now that you have your 3 container rpms, you can move on to the next step

# STEP 2 : Install and play with your containers
Heres a cool aspect to these containers, they don't need anything special to run
them, above any beyond rpm and systemd.  Since they're rpms, you can just
install them as you see fit. Lets do that now, run the following command:

`rpm -ivh /output/dir/*.rpm`

Note that the install failed in the %post script.  This is due to the fact that
these containers are btrfs images that require a btrfs filesystem to install.

Fear not however, these are relocatable packages, so you can install them
anywhere.  Start by uninstaling them:

`rpm -e base httpd_container my_website`

Now installthem again with this command:

`rpm -ivh --prefix=/btrfs/fs /output/dir/*.rpm`
 
This will install all three of the rpms you generated in /btrfs/fs/containers.
Under taht directory you will see subdirectories called base, httpd_container,
and my_website, and in each of those, a containerfs subdiretory, where the container
filesystem lives.  You can start the my_website container with the following
command:
`systemd-nspawn -D /btrfs/fs/containers/my_website/containerfs --machine=test`

You will be presented with a login screen for your new running container!  You
can login and snoop around with the id/password combination root/redhat (as
specified in the container customization script).  Feel free to look around and
do as you like.  When you are ready to shut it down logout and disconnect from
the container by pressing ^] 3 times.  Then shutdown the container with the
command:

`machinectl poweroff test`

You may have noted during your exploration of the container that it was running
in a degraded state.  This is due to the fact that no networking interfaces were
added to the container, preventing the web server process from starting
properly.  We will adress this later

# STEP 3 : Create a container repository

This step is fairly easy.  Using the containers you've created, build a yum/dnf
respository as you would for any other set of rpms.  Place the rpms in a
location of your choosing and run this command:

`createrepo /path/to/rpms`

This will generate all the metadata needed to serve as a yum/dnf repository

Next, that directory needs to be exposed to the network.  For this you will need
a http or ftp server.  The configuration of such a service is left as an
exercise to the reader.  Configure the server of your choosing to provide access
to the repository you have created, and record the url for later use

You can test the functionality of this repository by adding it to your
yum.repos.d directory, and attempting to install a container with the following
command:

`dnf install my_website`

The package and its dependencies should be automatically pulled in and
installed.  Don't forget to dnf erase them when you are done, and remove the
repository from your regular configuration

# STEP 4: Set up a freight agent

Now that we have our packages available for use, its time to setup our Freight
server.  This tutorial assumes that you are setting up a server for local use,
and as such we will us sqlite as our database.  Install the freight software,
and run the following command:

`createfreightdb-sqlite.sh /var/lib/freight-agent/fr.db`

This will create the initial database for freight. Note if you installed freight
from a repository rather than building it yourself, this may have already been
done for you).

Next, you need to create a tennant for yourself.  Run the following command:

`createtennant-sqlite.sh /var/lib/freight-agent/fr.db t1 tp tpp f`

This creates a tennant named t1 with a password of tp a proxy password of tpp
and is flagged as non-administrative.

Next you will need to configure your control interface.  Create a file
/home/<user>/.freightctl with the following contents:

> db = {
>         dbtype = "sqlite";
>         dbname = "/var/lib/freight-agent/fr.db";
>         user = "t1";
>         password = "tp";
> };

This provides access to the freight database.

Now you can make modifications to it.  Run the following command:

`freightctl repo add myrepo <url of repo from STEP 3>`

That tells freight that any containers you want to run should be searched for in
the repository you created in STEP 3.  You can see your configured repositories
with the `freightctl repo list` command.

Next, you need to assign a host.  For now we are only using localhost, so add it
with this command:

`freightctl host add localhost`

And subscribe yourself to use it:

`freightctl host subscribe localhost t1`

# STEP 5: Configure the agent to listen to the database

First we need to create a way for containers to reach the outside world.  For
that you need to make sure that you have a bridge connected to a production
network.  For this example we assume you have done this and named said bridge
br0

Now we need to run an agent to listen for container requests to the database next

Create the file /etc/freight-agent/config with the following contents:
> db = {
>         dbtype = "sqlite";
>         dbname = "/root/git/freight-tools/scripts/fr.db";
>         user = "fn";
>         password = "np";
> };
> 
> node = {
>         container_root = "/btrfs/fs/agent";
>         host_bridge = "br0";
>         hostname = "localhost";
> };

We need to initalize the agent first.  Do so by running the following command:

`freight-agent -m init`

This will take a moment to run as it poulates the container root with needed
utilities

Once complete you can start the agent with the following command:

`freight-agent -m node`

It will remain in the foreground, so you'll have to open another terminal to
continue with the next step:

# STEP 6: Define a network

One of the features of Freight is its integrated networking.  Multiple networks
can be defined at associated with containers as a given tennant sees fit.  Lets
define a very basic bridged network for our container.  Create the file
bridged.conf with the following contents:

> network = {
>         type = "bridged";
> };

> address_config = {
>         ipv4_aquisition = "dhcp";
>         ipv6_aquisition = "slaac";
> };

Then run the following command:

`freightctl network create testnet ./bridged.conf`

That creates a bridged network type that bridges any containers attached to it
to the production network reachable via br0 (as specified in the agent
configuration)

# STEP 7: Create a container

Now we can create a container instance.  Run the following command:

`freightctl container create my_website testsite localhost`

This tells freight that we want to create a container using the my_website rpm
with a name of testsite on the localhost agent.

Next, lets attach it to a network with the following command:

`freightctl network attach testsite testnet`

This tells freight that when we boot the testsite container it should be
attached to the testnet network

Now all thats left is to boot the container.  Run this command:

`freightctl container boot testsite`

This will start the install/boot process for the given container.  You can check
on its progress with this command:

`freightctl container list`

It will show its state as installing.  When the container is finished installing
it will move to the running state, at which point you can log into it and
explore it with the command:

`machinectl login testsite`

You can exit the container tty anytime just as you did in STEP 2.  This time
however, note that the container is running properly and available on the
network.  if you run the command:

`ip addr show`

in the container, you can see the ip address of the interface the container has
received.

Using that address, you can point your browser at it, and see the default web
page that your container is serving!

Congradulations!  You've setup Freight!

# Next steps : True multi-tennancy and clustering

This tutorial was meant to show you a basic overview of Freight.  Using
freightproxy and a Postgres database will allow you to scale your container
cluster up to an arbitrary number of agents and multiple tennants.  Please feel
free to explore and direct questions to me at nhorman@tuxdriver.com

Happy Shipping!



