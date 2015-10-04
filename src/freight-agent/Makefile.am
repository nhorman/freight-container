AM_CFLAGS = -Wall -O2 -D_GNU_SOURCE \
${libpq_CFLAGS} ${libconfig_CFLAGS} \
-I$(top_srcdir)/src/include/

AM_LDFLAGS = ${libpq_LIBS} ${libconfig_LIBS}

freightproxy_CFLAGS = ${AM_CFLAGS} ${xmlrpc_CFLAGS}\
		      ${xmlrpc_abyss_CFLAGS}\
		      ${xmlrpc_util_CFLAGS}

freightproxy_LDFLAGS = ${AM_LDFLAGS} ${xmlrpc_LIBS}\
		       ${xmlrpc_abyss_LIBS}\
		       ${xmlrpc_util_LIBS}\
		       ${xmlrpc_client_LIBS}\
		       ${xmlrpc_server_LIBS}\
		       ${xmlrpc_server_abyss_LIBS}

freightctl_CFLAGS = ${AM_CFLAGS} ${xmlrpc_client_CFLAGS}

freightctl_LDFLAGS = ${AM_LDFLAGS} ${xmlrpc_client_LIBS}\
		     ${xmlrpc_util_LIBS}\
		     ${xmlrpc_LIBS}

freight_agent_LDFLAGS = ${AM_LDFLAGS} ${xmlrpc_client_LIBS}\
			${xmlrpc_util_LIBS}\
			${xmlrpc_LIBS}

bin_PROGRAMS = freight-agent freightctl freightproxy

freight_agent_SOURCES = main.c freight-config.c\
			postgres-db.c mode.c nodb.c\
			freight-db.c xmlrpc_client.c\
			../freight-common.c

freightctl_SOURCES = freightctl_main.c \
		freight-config.c postgres-db.c\
		nodb.c freight-db.c xmlrpc_client.c\
		../freight-common.c

freightproxy_SOURCES = freightproxy_main.c freight-config.c \
		       freight-db.c ../freight-common.c postgres-db.c \
		       nodb.c proxy_handlers.c xmlrpc_client.c

