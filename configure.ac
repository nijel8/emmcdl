#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT(emmcdl, 1.0, cchua.cn@gmail.com)
AC_CONFIG_SRCDIR([inc/diskwriter.h])
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS(stdlib.h string.h termios.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS(memset strstr)
AC_PROG_RANLIB


AC_ARG_ENABLE([sysdeps],
[  --enable-sysdeps Turn on debugging],
[case "${enableval}" in
  yes) sysdeps=true ;;
  no)  sysdeps=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-sysdeps]) ;;
esac],[sysdeps=false])
AM_CONDITIONAL([SYSDEPS_WIN32], [test x$sysdeps = xtrue])


AC_OUTPUT(Makefile)
