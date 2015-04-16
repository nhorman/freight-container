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
ADMINUSER=$3
ADMINPASS=$4

usage() {
	echo "./createfreghtdb.sh <dbname> <nodeuser> <admn user> <pw>"
	echo "dbname - name of the database to use"
	echo "node user - user that freight nodes access the db with"
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

CREATE ROLE $NODEUSER NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN;

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


#
# create the appropriate tables
#
psql -h 127.0.0.1 -W $DBNAME $ADMINUSER << EOF
\x

CREATE TYPE status as ENUM ('operating', 'unreachable');

CREATE TABLE nodes (
	hostname	varchar(256) PRIMARY KEY,
	state		status NOT NULL
);

EOF


if [ $? -ne 0 ]
then
	echo "Error creating tables, aborting"
	psql << EOF
\x
DROP DATABSAE $DBNAME;
DROP ROLE $NODEUSER;
DROP ROLE $ADMINUSER;
EOF
	exit 0
fi

