#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([freight-tools], [0.1], [nhorman@tuxdriver.com])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_SRCDIR([src/freight-builder/manifest.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

AC_DEFINE([HAVE_GETOPT_LONG], [], [enable for getop_long support])
AC_DEFINE([HAVE_NFTW], [], [enable for nftw support])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h ftw.h])
# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
PKG_CHECK_MODULES([libpq], [libpq], [], [AC_MSG_ERROR([libpq is required])])
PKG_CHECK_MODULES([libconfig], [libconfig], [], [AC_MSG_ERROR([libconfig is required])])
PKG_CHECK_MODULES([xmlrpc_abyss], [xmlrpc_abyss], [], [AC_MSG_ERROR([xmlrpc_abyss is required])])

AC_CHECK_FUNC([getopt_long], [AC_DEFINE([HAVE_GETOPT_LONG],[1],[getopt_long support])])
AC_CHECK_FUNC([nftw], [AC_DEFINE([HAVE_NFTW],[1],[nftw support])], [
			AC_MSG_ERROR([unable to find the nftw() function])])
AC_OUTPUT(src/Makefile src/freight-builder/Makefile src/freight-agent/Makefile src/tests/Makefile)
