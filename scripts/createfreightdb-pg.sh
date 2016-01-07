#!/bin/sh
###################################################################
# createfreightdb.sh
#
# Shell script to initalize the postgres database for use with a
# Freight instance
#
# Usage ./createfreightdb.sh <dbname> <node user> <admin user> <pw>
###################################################################

DBNAME=$1
NODEUSER=$2
NODEPASS=$3
ADMINUSER=$4
ADMINPASS=$5

usage() {
	echo "./createfreghtdb.sh <dbname> <nodeuser> <node pass> <admn user> <pw>"
	echo "dbname - name of the database to use"
	echo "node user - user that freight nodes access the db with"
	echo "node pass - password that freight nodes access the db with"
	echo "admin user - user that freightctl access the db with"
	echo "pw - password for the admin user"
}


if [ -z "$DBNAME" ]
then
	usage
	exit 0
fi

if [ -z "$NODEUSER" ]
then
	usage
	exit 0
fi

if [ -z "$NODEPASS" ]
then
	usage
	exit 0
fi

if [ -z "$ADMINUSER" ]
then
	usage
	exit 0
fi

if [ -z "$ADMINPASS" ]
then
	usage
	exit 0
fi

#
# Create the initial database and roles
#
psql << EOF
\x
CREATE DATABASE $DBNAME;

CREATE ROLE $NODEUSER PASSWORD '$NODEPASS' NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN;

CREATE ROLE $ADMINUSER PASSWORD '$ADMINPASS' NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN;

GRANT ALL ON DATABASE $DBNAME to $ADMINUSER;

GRANT ALL ON DATABASE $DBNAME to $NODEUSER;

EOF

if [ $? -ne 0 ]
then
	echo "Error creating roles/db, aborting"
	psql << EOF
\x
DROP DATABSAE $DBNAME;
DROP ROLE $NODEUSER;
DROP ROLE $ADMINUSER;
EOF
	exit 0
fi

export PGPASSWORD=$ADMINPASS

#
# create the appropriate tables
#
psql -h 127.0.0.1 -w $DBNAME $ADMINUSER << EOF
\x

CREATE OR REPLACE FUNCTION update_modified_column()
RETURNS TRIGGER AS \$\$
BEGIN
	NEW.modified = now();
	return NEW;
END;
\$\$ LANGUAGE plpgsql;

CREATE TYPE status as ENUM ('offline', 'operating', 'unreachable');
CREATE TYPE cstate as ENUM ('staged', 'start-requested', 'failed', 'installing', 'running', 'exiting');
CREATE TYPE nstate as ENUM ('staged', 'active', 'failed');

CREATE TABLE global_config (
	key	varchar(512) NOT NULL PRIMARY KEY,
	value   varchar NOT NULL
);

INSERT INTO global_config VALUES('db_version', '2');
INSERT INTO global_config VALUES('base_interval', '30');
INSERT INTO global_config VALUES('healthcheck_multiple', '1');
INSERT INTO global_config VALUES('gc_multiple', '2');

CREATE TABLE tennants (
	tennant	varchar(512) NOT NULL PRIMARY KEY,
	proxypass varchar(512),
	proxyadmin boolean
);

CREATE TABLE nodes (
	hostname	varchar(256) PRIMARY KEY,
	state		status NOT NULL,
	load		integer,
	modified	timestamp
);

CREATE TRIGGER update_node_modtime BEFORE UPDATE ON nodes
 FOR EACH ROW EXECUTE PROCEDURE update_modified_column();

CREATE TABLE tennant_hosts (
	hostname	varchar(512) NOT NULL references nodes(hostname),
	tennant		varchar(512) NOT NULL references tennants(tennant),
	PRIMARY KEY (hostname, tennant)
);

CREATE TABLE yum_config (
	name	varchar(32) NOT NULL,
	url	varchar(512) NOT NULL,
	tennant varchar(512) NOT NULL references tennants(tennant)
);

CREATE TABLE containers (
	tennant         varchar(512) NOT NULL references tennants(tennant),
	iname           varchar(512) NOT NULL,
	cname           varchar(512) NOT NULL,
	hostname        varchar(512) NOT NULL references nodes(hostname),
	state		cstate NOT NULL,
	PRIMARY KEY (tennant, iname)
);

CREATE TABLE networks (
	name		varchar(512) NOT NULL,
	tennant		varchar(512) NOT NULL references tennants(tennant),
	state		nstate NOT NULL,
	config		varchar NOT NULL,
	PRIMARY KEY (tennant, name)
);

CREATE TABLE net_container_map (
	tennant		varchar(512) NOT NULL,
	name		varchar(512) NOT NULL,
	network		varchar(512) NOT NULL,
	PRIMARY KEY (tennant, name, network),
	FOREIGN KEY (tennant, name) references containers(tennant, iname),
	FOREIGN KEY (tennant, network) references networks(tennant, name)
);

	

GRANT ALL on tennants to $ADMINUSER;
GRANT ALL on nodes to $ADMINUSER;
GRANT ALL on tennant_hosts to $ADMINUSER;
GRANT ALL on yum_config to $ADMINUSER;
GRANT ALL on containers to $ADMINUSER;
GRANT ALL on networks to $ADMINUSER;
GRANT ALL on net_container_map to $ADMINUSER;
GRANT ALL on global_config to $ADMINUSER;

GRANT ALL on tennants to $NODEUSER;
GRANT ALL on nodes to $NODEUSER;
GRANT ALL on tennant_hosts to $NODEUSER;
GRANT ALL on yum_config to $NODEUSER;
GRANT ALL on containers to $NODEUSER;
GRANT ALL on networks to $NODEUSER;
GRANT SELECT on net_container_map to $NODEUSER;
GRANT SELECT on global_config to $NODEUSER;

EOF


if [ $? -ne 0 ]
then
	echo "Error creating tables, aborting"
	psql << EOF
\x
DROP DATABASE $DBNAME;
DROP ROLE $NODEUSER;
DROP ROLE $ADMINUSER;
EOF
	exit 0
fi

