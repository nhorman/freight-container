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
 *File: freight-common.c
 *
 *Author: Pavel Odvody
 *
 *Date: 6/1/2015
 *
 *Description: common utility routines for freight tools 
 *********************************************************/

#include <freight-common.h>

char *strvjoin(const char **strv, const char *separator) {
	size_t s = 0, d = 0;
	unsigned i, k;
	char *p, *q;

	for (k = 0; strv[k]; ++k) {
		s += strlen(strv[k]);
		// catch overflow
		if (d >= s) 
			return NULL;
		d = s;
	}

	s += (strlen(separator) * (k - 1)) + 1;
	p = q = malloc(s);
	if (!p)
		return NULL;

	for (i = 0; strv[i]; ++i) {
		q = stpcpy(q, strv[i]);
		if (i != (k-1))
			q = stpcpy(q, separator);	
	}
	*++q = 0;

	return p;
}

char *strjoin(const char *a, ...) {
        va_list l;
        size_t s = 0, d = 0;
        const char *p = NULL;
	char *r = NULL, *x = NULL;

        if (!a)
                return NULL;

        va_start(l, a);
        s += strlen(a);
	d = s;

        for(;;) {
                bool overflow;
                p = va_arg(l, const char *);
                if (!p)
                        break;

                s += strlen(p);
                if (d >= s) {
                        va_end(l);
                        return NULL;
                }

                d = s;
        }

        va_end(l);
        x = r = malloc(s + 1);
        if (!r)
                return NULL;

        r = stpcpy(r, a);
        va_start(l, a);
        for (;;) {
                p = va_arg(l, const char *);
                if (!p)
                        break;
                r = stpcpy(r, p);
        }
        va_end(l);
        *r = 0;

        return x;
}

inline size_t s_max(size_t a, size_t b) {
	return a > b ? a : b;
}

bool streq(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}

void *my_realloc(void **b, size_t *size, size_t desired, size_t unit) {
	size_t newsize;
	void *q;

	assert(b);
	assert(size);

	if (*size > desired)
		return b;

	newsize = s_max(desired * 2, 64/unit);
	if ((newsize * unit) < (desired * unit))
		return NULL;

	q = realloc(*b, newsize * unit);
	if (!q)
		return NULL;

	*b = q;
	*size = newsize;
	return q;
}

char *strsepjoin(const char* s, ...) {
	va_list l;
	__free const char **p = NULL;
	const char *b = NULL;
	size_t x = 0, v = 0;

	if (!s)
		return NULL;

	va_start(l, s);

	for (;;) {
		b = va_arg(l, const char *);
		if (!b)
			break;

		if (!__realloc(p, x, v + 2)) {
			va_end(l);
			return NULL;
		}

		p[v] = b;
		p[v+1] = NULL;
		v++;
	}

	va_end(l);
	
	return strvjoin(p, s);
}
