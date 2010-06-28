#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([include/TextSection.h])
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/acx_mpi.m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_FC
AC_PROG_INSTALL

AC_LANG([C])
ACX_MPI

AC_ARG_ENABLE(static, AC_HELP_STRING([--enable-static], [Use static archives for instrumentation libraries [yes|no]]), ok=$enableval)
if test "$ok" = "yes"; then
   STATIC_INST_LIB="yes"
fi

AC_SUBST(STATIC_INST_LIB)

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero memset])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tools/Makefile
                 testapps/Makefile
                 instcode/Makefile])
AC_CONFIG_SUBDIRS([external/udis86-1.7])
AC_OUTPUT
