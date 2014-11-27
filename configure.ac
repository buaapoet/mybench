#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mybench], [0.1], [haorenpingan030@126.com])
AC_CONFIG_SRCDIR([mybench.cpp])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB(curl, curl_global_init, [], [echo "Error! You need to have libcurl installed!" exit -1])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
