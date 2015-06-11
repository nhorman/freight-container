AM_CFLAGS = -Wall -O2 -D_GNU_SOURCE \
${libconfig_CFLAGS} -I$(top_srcdir)/src/include/

AM_LDFLAGS = ${libconfig_LIBS}

bin_PROGRAMS = test-int

test_int_SOURCES = test-int.c ../freight-common.c
