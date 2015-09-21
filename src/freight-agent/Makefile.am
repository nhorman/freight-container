AM_CFLAGS = -Wall -O2 -D_GNU_SOURCE \
${libpq_CFLAGS} ${libconfig_CFLAGS} \
-I$(top_srcdir)/src/include/

AM_LDFLAGS = ${libpq_LIBS} ${libconfig_LIBS}

freightproxy_CFLAGS = ${AM_CFLAGS} ${xmlrpc_abyss_CFLAGS}

freightproxy_LDFLAGS = ${AM_LDFLAGS} ${xmlrpc_abyss_LIBS}

bin_PROGRAMS = freight-agent freightctl freightproxy

freight_agent_SOURCES = main.c freight-config.c\
			postgres-db.c mode.c nodb.c\
			freight-db.c ../freight-common.c

freightctl_SOURCES = freightctl_main.c \
		freight-config.c postgres-db.c\
		nodb.c freight-db.c ../freight-common.c

freightproxy_SOURCES = freightproxy_main.c freight-config.c \
		       freight-db.c ../freight-common.c postgres-db.c \
		       nodb.c proxy_handlers.c

