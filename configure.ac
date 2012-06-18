#
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/ElfFileInst.C])
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/acx_mpi.m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_FC
AC_PROG_CPP

AC_ARG_ENABLE(mpi, AC_HELP_STRING([--enable-mpi], [Use mpi compiler when building instrumentation libraries [yes]]), mpi_ok=$enableval)
if test "$mpi_ok" = "no"; then
   MPICC="$CC"
   MPIFC="$FC"
   MPICXX="$CXX"
   MPI_FLAGS=""
else
   AC_LANG([C])
   ACX_MPI
   AC_LANG_PUSH([Fortran])
   ACX_MPI
   AC_LANG_POP
   AC_LANG_PUSH(C++)
   ACX_MPI
   AC_LANG_POP()
   MPI_FLAGS="-DHAVE_MPI"
fi

AC_ARG_ENABLE(static, AC_HELP_STRING([--enable-static], [Use static archives for instrumentation libraries; it is extremely unlikely that you want to use this [no]]), static_ok=$enableval)
if test "$static_ok" = "yes"; then
   STATIC_INST_LIB="yes"
fi
AC_SUBST(STATIC_INST_LIB)

AC_ARG_ENABLE(instruction_trace, AC_HELP_STRING([--enable-instruction-trace], [Collect simulation stats per instruction, affects jbb and sim only. [no]]), perinsn_ok=$enableval)
if test "$perinsn_ok" = "yes"; then
   MPI_FLAGS="$MPI_FLAGS -DSTATS_PER_INSTRUCTION"
fi

AC_ARG_ENABLE(mpiinit_check, AC_HELP_STRING([--enable-mpiinit-check], [Disable output file printing in inst libraries unless mpi_init is executed [no]]), mpiinit_check_ok=$enableval)
if test "$mpiinit_check_ok" = "yes"; then
   MPI_FLAGS="$MPI_FLAGS -DMPI_INIT_REQUIRED"
fi

AC_SUBST(MPI_FLAGS)

AC_ARG_WITH(throttler_headers,
            [AS_HELP_STRING([--with-throttler-headers=<path>],[specify path to throttler headers])],
            [throttler_headers=$withval
             AC_CHECK_HEADER([$throttler_headers/throttler.h],
                             [THROTTLER_FLAGS="-DHAVE_THROTTLER_H -I$throttler_headers"],
                             [AC_MSG_ERROR(throttler.h not found using $throttler_headers)])],
            [throttler_headers=no])
AC_SUBST(THROTTLER_FLAGS)

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h unistd.h])
AC_CHECK_HEADERS([cpufreq.h],
        CPUFREQ_FLAGS="-DHAVE_CPUFREQ_H")
AC_SUBST(CPUFREQ_FLAGS)

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
                 instcode/Makefile
                 scripts/blacklist.sh
                 cshrc
                 bashrc])
AC_CONFIG_SUBDIRS([external/udis86-1.7])
AC_OUTPUT
chmod +x scripts/blacklist.sh

exit 0
