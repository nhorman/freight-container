# How to Build and Manage a Freight Container Environment

This document exists to outline the general details on how to implement a freight container environment


## Architectural Overview
Freight Is made up of several components which interact (generally speaking)
according to the following diagram:

						    +------------+               
						    |  admin     |               
						    |  freightctl|               
						    +---+--------+               
							|                        
	+-----------------+       +-------------+    +--v--------+               
	|    tennant      |       |             |    |           |               
	|    freightctl   +-----> |             |    | postgres  |               
	|                 |       |             |    +------+----+------+        
	+-----------------+       |             +-----^     |           |        
				  | freightproxy|           |           |        
	+-----------------+       |             |    +------v----+  +---v-------+
	|    tennant      |       |             |    | freight-  |  |freight-   |
	|    freightctl   +-----> |             |    | agent node|  |agent node |
	|                 |       |             |    |           |  |           |
	+-----------------+       +-------------+    +----+------+  +-----+-----+
							  ^               ^      
				  +-------------+         |               |      
				  |             +---------+               |      
				  |             |                         |      
				  | yum repo    |                         |      
				  |             +-------------------------+      
				  |             |                                
				  +-------------+                                


This diagram lays out all the existing components of Freight:
* freightctl
* freightproxy
* A postgres database
* freght-agent
* A yum repository

Freight is a multitennant environment, meaning that multiple independent users
can schedule containers on the same freight cluster independently.


### Frieghtctl
Freightctl is the primary utilty used by tennants in the freightctl environment
to configure their environment, create/delete/manage containers, and query the
freight cluster about host status.  Administrators also use freigthctl for the
aforementioned uses, as well as the management of hosts in the cluster and the
subscription of said hosts for use by specific tennants.  Tennant instances of
freightctl should be configured to use freightproxy for cluster communication,
while admin instances should use the postgres configuration


### Freightproxy
Freighproxy is an xmlrpc server which proxies requests from a freightctl
instance to the postgres database.  Freightproxy provides authentication to
ensure that given tennants use is valid, and restricts requests so that tennants
remain isolated.

Configuration for freightproxy should point to the postgres database in the db
section, and should configure a port to listen on in the proxy section


### Postgres database
This is a standard postgres database.  It should be configured to use password
authentication.  There are 2 scripts in the scripts subdirectory related to
configuring the database:

* createfreightdb
* createtennant

createfreightdb creates all the needed roles/tables/etc to run a freight
cluster.  It requires that you specify the database name, the freight agent
node and password as well as the admin user and password.  The former will be
used when configuring freight agent nodes, while the latter is used by
administrators running freightctl

createtennant allows for the creation of a tennant for use by tennant instances
running freightctl.  The script requires that you provide a tennant name and
password (should you desire to allow tennants direct access to the database,
which is not recommended), as well as the tennants freightproxy password.  This
script should be run for each tennant.

### Yum Respository
This is a standard web service that holds collections of rpms.  For this purpose
however, the rpm repositories are collections of containers created via the
freight-builder utility.  Container rpms should be uploaded here, and each
tennant should configure the repository in the cluster using the freightctl add
repo command

### Freight-agent nodes
These are physical or virtual systems which run the freight-agent daemon and
monitor the postgres database for requests to create/run new containers from
tennants. nodes should be configured to point to the postgres database using the
node user and password created via the createfreightdb script

