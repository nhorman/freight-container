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

CREATE TYPE status as ENUM ('offline', 'operating', 'unreachable');
CREATE TYPE cstate as ENUM ('new', 'failed', 'installing', 'running', 'exiting');

CREATE TABLE tennants (
	tennant	varchar(512) NOT NULL PRIMARY KEY
);

CREATE TABLE nodes (
	hostname	varchar(256) PRIMARY KEY,
	state		status NOT NULL
);

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
	PRIMARY KEY (tennant, iname)
);

GRANT ALL on tennants to $ADMINUSER;
GRANT ALL on nodes to $ADMINUSER;
GRANT ALL on tennant_hosts to $ADMINUSER;
GRANT ALL on yum_config to $ADMINUSER;
GRANT ALL on containers to $ADMINUSER;

GRANT ALL on tennants to $NODEUSER;
GRANT ALL on nodes to $NODEUSER;
GRANT ALL on tennant_hosts to $NODEUSER;
GRANT ALL on yum_config to $NODEUSER;
GRANT ALL on containers to $NODEUSER;
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

