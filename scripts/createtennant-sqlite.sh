#!/bin/sh
###################################################################
# createfreightdb.sh
#
# Shell script to initalize the postgres database for use with a
# Freight instance
#
# Usage ./createtennant-sqlite.sh <file> <tennant> <pw> <proxypw>
###################################################################

DBNAME=$1
TENNANT=$2
TENNANTPASS=$3
PROXYPASS=$4
ADMIN=$5

usage() {
	echo "./createtennant-sqlite.sh <file> <tennant> <pw> <proxypw> <admin>"
	echo "dbname - name of the database to use"
	echo "tennant - the tennant name to create"
	echo "tennantpw - the password for the new tennant"
	echo "proxypass - the proxy password for the new tannant"
	echo "admin - [t|f]: tennant is an admin"
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

#
# create the appropriate tennant entry 
#
sqlite3 $DBNAME << EOF

INSERT into tennants VALUES ('$TENNANT', '$PROXYPASS', '$ADMIN');

EOF

