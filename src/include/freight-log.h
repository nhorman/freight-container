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
 * *File: log.h
 * *
 * *Author:Neil Horman
 * *
 * *Date: April 16, 2015
 * *
 * *Description log macros for recording errors
 * *********************************************************/


#ifndef _FREIGHT_LOG_H_
#define _FREIGHT_LOG_H_
#include <stdio.h>
#include <syslog.h>

#define LOG(prio, format, args...) do {\
	fprintf(stderr, #prio": "  format, ##args);\
} while(0)

#endif
