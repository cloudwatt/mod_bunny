#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mod_bunny], [0.5.1], [])
AC_CONFIG_SRCDIR([src/mb_json.c])
AM_INIT_AUTOMAKE
LT_INIT
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])

LIBMB_MAJOR=0
LIBMB_MINOR=0
LIBMB_MICRO=0
AC_SUBST(LIBMB_MAJOR)
AC_SUBST(LIBMB_MINOR)
AC_SUBST(LIBMB_MICRO)

AC_ARG_WITH(nagios-include,
[AS_HELP_STRING([--with-nagios-include=PATH],
		[specify prefix directory for Nagios include files])])
if test "x$with_nagios_include" != x; then
  NAGIOS_INCLUDE="-I$with_nagios_include"
  CPPFLAGS="$CPPFLAGS $nagios_include"
  AC_SUBST(NAGIOS_INCLUDE)
fi

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h sys/time.h unistd.h])
AC_CHECK_HEADER([amqp.h], [], [AC_MSG_ERROR(Cannot find amqp.h.)])
AC_CHECK_HEADER([jansson.h], [], [AC_MSG_ERROR(Cannot find jansson.h.)])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset strdup])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
