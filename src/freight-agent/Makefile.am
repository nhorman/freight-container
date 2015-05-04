AM_CFLAGS = -Wall -O2 -D_GNU_SOURCE \
${libpq_CFLAGS} ${libconfig_CFLAGS} \
-I$(top_srcdir)/src/include/

AM_LDFLAGS = ${libpq_LIBS} ${libconfig_LIBS}

bin_PROGRAMS = freight-agent freightctl

freight_agent_SOURCES = main.c freight-config.c\
			postgres-db.c mode.c nodb.c

freightctl_SOURCES = freightctl_main.c \
		freight-config.c postgres-db.c\
		nodb.c
