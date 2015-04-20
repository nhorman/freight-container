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
 * *File: postgres-db.c
 * *
 * *Author:Neil Horman
 * *
 * *Date:
 * *
 * *Description implements access to postgres db 
 * *********************************************************/

#include <stdio.h>
#include <freight-log.h>
#include <freight-db.h>


static int pg_connect(struct agent_config *acfg)
{
	return 0;
}

static int pg_disconnect(struct agent_config *acfg)
{
	return 0;
}

struct db_api postgres_db_api {
	.connect = pg_connect,
	.disconnect = pg_disconnect,
};
