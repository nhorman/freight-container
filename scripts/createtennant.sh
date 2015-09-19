#!/bin/sh
###################################################################
# createfreightdb.sh
#
# Shell script to initalize the postgres database for use with a
# Freight instance
#
# Usage ./createfreightdb.sh <dbname> <node user> <admin user> <pw> <proxypw>
###################################################################

DBNAME=$1
ADMINUSER=$2
ADMINPASS=$3
TENNANT=$4
TENNANTPASS=$5
PROXYPASS=$6

usage() {
	echo "./createfreghtdb.sh <dbname> <nodeuser> <node pass> <admn user> <pw>"
	echo "dbname - name of the database to use"
	echo "admin user - user that freightctl access the db with"
	echo "pw - password for the admin user"
	echo "tennant - the tennant name to create"
	echo "tennantpw - the password for the new tennant"
}


if [ -z "$DBNAME" ]
then
	usage
	exit 0
fi

if [ -z "$TENNANT" ]
then
	usage
	exit 0
fi

if [ -z "$TENNANTPASS" ]
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
psql $DBNAME << EOF
\x

CREATE ROLE $TENNANT PASSWORD '$TENNANTPASS' NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN;
GRANT ALL ON tennants to $TENNANT;
GRANT SELECT ON nodes to $TENNANT;
GRANT INSERT ON yum_config to $TENNANT;
GRANT DELETE ON yum_config to $TENNANT;
GRANT SELECT ON yum_config to $TENNANT;
GRANT SELECT ON tennant_hosts to $TENNANT;
GRANT SELECT ON containers to $TENNANT;
GRANT INSERT ON containers to $TENNANT;
GRANT UPDATE ON containers to $TENNANT;
GRANT DELETE ON containers to $TENNANT;

EOF

if [ $? -ne 0 ]
then
	echo "Error creating tennant, aborting"
	exit 0
fi

export PGPASSWORD=$TENNANTPASS

#
# create the appropriate tennant entry 
#
psql -h 127.0.0.1 -w $DBNAME $TENNANT << EOF
\x

INSERT into tennants VALUES (current_user, '$PROXYPASS');

EOF


if [ $? -ne 0 ]
then
	echo "Error Adding tennant, aborting"
	psql << EOF
\x
DROP ROLE $TENNANT;
EOF
	exit 0
fi

psql $DBNAME << EOF
\x

GRANT SELECT ON tennants to $TENNANT;
EOF
