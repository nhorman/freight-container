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

CREATE TYPE status as ENUM ('operating', 'unreachable');

CREATE TABLE tennants (
	tennant	varchar(512) NOT NULL PRIMARY KEY
);

CREATE TABLE nodes (
	hostname	varchar(256) PRIMARY KEY,
	state		status NOT NULL
);

CREATE TABLE yum_config (
	name	varchar(32) NOT NULL,
	url	varchar(512) NOT NULL
);

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

