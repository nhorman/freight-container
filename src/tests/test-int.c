/*********************************************************
 *Copyright (C) 2015 Pavel Odvody
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
 *File: test-int.c
 *
 *Author: Pavel Odvody
 *
 *Date: 26.5.2015
 *
 *Description
 *********************************************************/

#include <freight-common.h>

const char *s[] = {
	"abcd",
	"test",
	"blah blah",
	"string",
	"aaaaaa",
	"weweweweew",
	NULL
};

void test(int r, const char* header) {
	if (r == 0) {
		printf(" \033[1;32mPASS:\033[0m   %s\n", header);
	} else {
		printf(" \033[1;31mFAILED:\033[0m %s: %s\n", header, strerror(-r));
	}
}


void test_ab(const char *a, const char* b) {
	if (!streq(a, b))
		printf(" \033[1;31mFAILED:\033[0m streq \n\t '%s' != '%s'\n", a, b);
	else
		printf(" \033[1;32mPASS:\033[0m   streq \n\t '%s' == '%s'\n", a, b);
}

int test_strsepjoin() {
	char *a = NULL, *b = NULL;

	a = "abcd-->blah blah-->aaaaaa";
	b = strsepjoin("-->", s[0], s[2], s[4], NULL);
	test_ab(a, b);

	return 0;
}

int test_strjoin() {
	char *a = NULL, *b = NULL;

	a = "abcd + blah blah";
	b = strjoina(s[0], " + ", s[2]);
	test_ab(a, b);

	a = "weweweweewaaaaaa|string";
	b = strjoina(s[5], s[4], "|", s[3]);
	test_ab(a, b);

	a = "abcd + blah blah";
	b = strjoin(s[0], " + ", s[2], NULL);
	test_ab(a, b);

	a = "weweweweewaaaaaa|string";
	b = strjoin(s[5], s[4], "|", s[3], NULL);
	test_ab(a, b);

	a = "abcd:)test:)blah blah:)string:)aaaaaa:)weweweweew";
	b = strvjoin(s, ":)");
	test_ab(a, b);

	return 0;
}

int print_strv(const char **p) {
	unsigned i;
	assert(p);

	for(i = 0; p[i]; i++)
		printf("\t%u. %s\n", i+1, p[i]);

	return 0;
}

int test_realloc_cleanup() {
	unsigned i;
	size_t a = 0;
	__free_strv const char **strings = NULL;

	for (i = 0; i < 6; i++) {
		if(!__realloc(strings, a, i+1))
			return -ENOMEM;

		strings[i] = strdup(s[i]);
		strings[i+1] = NULL;
	}

	return 0;
}

typedef struct TestStruct {
	unsigned v;
	unsigned w;
	bool freed;
} TestStruct;

int new_test_struct(TestStruct** s, unsigned a, unsigned b) {
	TestStruct *p;

	p = alloc_zero(TestStruct, 1);
	if (!p)
		return -ENOMEM;

	p->v = a;
	p->w = b;

	*s = p;

	return 0;
}

int cmp_test_struct(TestStruct *s) {
	if (!s)
		return -EINVAL;

	if (s->v != 0xCAFEBABE)
		return -EINVAL;

	if (s->w != 0xDEADB347)
		return -EINVAL;

	if (s->freed)
		return -EINVAL;

	return 0;
}

void free_test_struct(TestStruct *s) {
	if (!s)
		return;

	s->freed = true;
}

CLEANUP_FUNC(TestStruct *, free_test_struct);
#define __free_test_struct __cleanup(free_test_structp)

int scope_out_test(TestStruct *ts) {
	__free_test_struct TestStruct *q = ts;
	return q->freed ? -EINVAL : 0;
}

int scope_check_freed(TestStruct *ts) {
	__free_test_struct TestStruct *q = ts;
	if (true) 
		q = NULL;
	return ts->freed ? -EINVAL : 0;
}

int main(int argc, char **argv)
{
	TestStruct* ts; 
	int r;

	(void) test_strjoin();
	(void) test_strsepjoin();

	r = test_realloc_cleanup();
	test(r, "realloc_cleanup");

	r = new_test_struct(&ts, 0xCAFEBABE, 0xDEADB347);
	test(r, "new_test_struct");

	r = cmp_test_struct(ts);
	test(r, "cmp_test_struct");

	r = scope_check_freed(ts);
	test(r, "scope_check_freed");

	r = scope_out_test(ts);
	test(r, "scope_in_test");

	r = ts->freed ? 0 : -EINVAL;
	test(r, "scope_out_test");

	return r;
}

