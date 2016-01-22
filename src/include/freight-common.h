/*********************************************************
 *Copyright (C) 2004 Neil Horman
 *This program is free software; you can redistribute it and\or modify
 *it under the terms of the GNU General Public License as published 
 *by the Free Software Foundation; either version 2 of the License,
 *or  any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *GNU General Public License for more details.
 *
 *File: freight-common.h 
 *
 *Author:Neil Horman
 *
 *Date: 4/9/2015
 *
 *Description: common utility routines for freight tools 
 *********************************************************/

#ifndef _FREIGHT_COMMON_H_
#define _FREIGHT_COMMON_H_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <ftw.h>
#include <freight-log.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define FREIGHT_DB_VERSION 2

#define __cleanup(x) __attribute__ ((cleanup(x)))
#define __free       __cleanup(freep)

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

static inline void freep(void *c) {
	free(*(void **) c);
}

static inline void freestrv(const char **c) {
	unsigned i;
	if (!c)
		return;	

	for (i = 0; c[i]; ++i)
		free((void *)c[i]);

	free((void *)c);
}

#define CLEANUP_FUNC(T, F) static inline void F##p(void* c) {\
	if (c) \
		F(*(T*)c);\
} typedef void* __semicolon_swallover

CLEANUP_FUNC(const char **, freestrv);
#define __free_strv __cleanup(freestrvp)

char *strvjoin(const char **strv, const char *separator);

#define strjoina(q, ...) ({\
const char* __items[] = {q, __VA_ARGS__ };\
unsigned __len = 0, __i = 0;\
char *__mem = NULL, *__nt = NULL;\
for(__i=0; __i < ARRAY_SIZE(__items) && __items[__i]; ++__i)\
        __len += strlen(__items[__i]);\
__mem = alloca(__len + 1);\
__nt = __mem; \
for(__i=0; __i < ARRAY_SIZE(__items) && __items[__i]; ++__i)\
        __nt = stpcpy(__nt, __items[__i]);\
__nt = 0;\
__mem;\
})

char *strjoin(const char *a, ...)  __attribute((sentinel));


#define strappend(s, a, ...) ({\
	char *___new = strjoin(s,a,  __VA_ARGS__, NULL);\
	free(s);\
	___new;\
})

static inline size_t s_max(size_t a, size_t b) {
        return a > b ? a : b;
}

bool streq(const char *a, const char *b);

void *my_realloc(void **b, size_t *size, size_t desired, size_t unit);

#define __realloc(what, size, desired) \
	my_realloc((void **)&(what), &(size), desired, sizeof((what)[0]))

#define alloc_zero(t, n) \
	calloc((n), sizeof(t))

char *strsepjoin(const char* s, ...) __attribute((sentinel));

static inline int __remove_path(const char *fpath, const struct stat *sb, int typeflag,
				struct FTW *ftwbuf)
{
	int rc;
	char *reason;
	struct stat buf;

	switch (typeflag) {
	case FTW_F:
	case FTW_SL:
	case FTW_SLN:
	case FTW_NS:
		rc = unlink(fpath);
		break;
	case FTW_DP:
	case FTW_D:
		lstat(fpath, &buf);
		if (buf.st_mode & S_IFLNK)
			rc = unlink(fpath);
		else
			rc = rmdir(fpath);
		break;
	default:
		rc = -EINVAL;
	}

	if (rc) {
		reason = strerror(errno);
		fprintf(stderr, "Could not remove %s: %s", fpath, reason);
	}

	return rc;
	
}

static inline void recursive_dir_cleanup(const char *path)
{
	nftw(path, __remove_path, 10, FTW_DEPTH|FTW_PHYS);
	return;
}

static inline FILE* _run_command(char *cmd)
{
	return popen(cmd, "r");
}

	
static inline int run_command(char *cmd, int print)
{
	int rc;
	FILE *cmd_out;
	char buf[128];
	
	cmd_out = _run_command(cmd); 
	if (cmd_out == NULL) {
		rc = errno;
		LOG(ERROR, "Unable to exec yum for install: %s\n", strerror(rc));
		return rc;
	}

	while (fgets(buf, 128, cmd_out)) {
		if (print)
			fputs(buf, stderr);
	}
 
	rc = pclose(cmd_out);

	if (rc == -1) {
		rc = errno;
		LOG(ERROR, "command failed: %s\n", strerror(rc));
	}

	return rc;
}

#endif
