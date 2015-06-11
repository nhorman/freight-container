AM_CFLAGS = -Wall -O2 -D_GNU_SOURCE \
${libconfig_CFLAGS} -I$(top_srcdir)/src/include/

AM_LDFLAGS = ${libconfig_LIBS}

bin_PROGRAMS = freight-builder

freight_builder_SOURCES = main.c manifest.c manifest.h package.h package.c yum.c ../freight-common.c

