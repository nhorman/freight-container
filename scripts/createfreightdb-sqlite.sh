#!/bin/sh
###################################################################
# createfreightdb.sh
#
# Shell script to initalize the postgres database for use with a
# Freight instance
#
# Usage ./createfreightdb.sh <file>
###################################################################

DBFILE=$1

usage() {
	echo "./createfreghtdb-sqlite.sh <file>"
	echo "file - the database file name"
}


if [ -z "$DBFILE" ]
then
	usage
	exit 0
fi

#
# create the appropriate tables
#
sqlite3 $DBFILE << EOF

CREATE TABLE status (status varchar(512) PRIMARY KEY NOT NULL);
INSERT INTO status values ('offline');
INSERT INTO status values ('operating');
INSERT INTO status values ('unreachable');

CREATE TABLE cstate (cstate varchar(512) PRIMARY KEY NOT NULL);
INSERT INTO cstate values('staged');
INSERT INTO cstate values('start-requested');
INSERT INTO cstate values('failed');
INSERT INTO cstate values('installing');
INSERT INTO cstate values('running');
INSERT INTO cstate values('exiting');

CREATE TABLE nstate (nstate varchar(512) PRIMARY KEY NOT NULL);
INSERT INTO nstate values('staged');
INSERT INTO nstate values('active');
INSERT INTO nstate values('failed');

CREATE TABLE global_config (
	key     varchar(512) NOT NULL PRIMARY KEY,
	value   varchar NOT NULL
);

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
	state 		varchar(512) references status(status),
	load		integer,
	modified	timestamp
);

CREATE TRIGGER [update_modified_time]
AFTER UPDATE
ON nodes
FOR EACH ROW
BEGIN
	UPDATE nodes SET modified=datetime('now', 'localtime') WHERE hostname = old.hostname;
END;

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
	state		varchar(512) references cstate(cstate),
	PRIMARY KEY (tennant, iname)
);

CREATE TABLE networks (
	name		varchar(512) NOT NULL,
	tennant		varchar(512) NOT NULL references tennants(tennant),
	state		varchar(512) references nstate(nstate),
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


CREATE TABLE event_table (
	channel varchar(512) NOT NULL,
	extra   varchar(512)
);

EOF


if [ $? -ne 0 ]
then
	echo "Error creating tables, aborting"
	exit 0
fi

