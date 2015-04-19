AM_CFLAGS = -Wall -O2 -D_GNU_SOURCE ${libpq_CFLAGS} ${libconfig_CFLAGS}

AM_LDFLAGS = ${libpq_LIBS} ${libconfig_LIBS}

bin_PROGRAMS = freight-agent

freight_agent_SOURCES = main.c freight-config.c

