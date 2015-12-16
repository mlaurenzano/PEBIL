#
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/ElfFileInst.C])
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/acx_mpi.m4])
m4_include([m4/ax_cxx_header_unordered_map.m4])
m4_include([m4/ax_compile_stdcxx_0x.m4])

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

   SAVECXX="$CXX"
   CXX="$MPICXX"
   MPI_CXXFLAGS="$CXXFLAGS"
   AC_COMPILE_STDCXX_0X
   AX_CXX_HEADER_UNORDERED_MAP
   if test "$ax_cv_cxx_unordered_map" = "yes"; then
      if test "$ac_cv_cxx_compile_cxx0x_gxx" = "yes"; then
         MPI_CXXFLAGS="-std=gnu++0x";
      elif test "$ac_cv_cxx_compile_cxx0x_cxx" = "yes"; then
         MPI_CXXFLAGS="-std=c++0x";
      else
         AC_MSG_ERROR([configure detected that unordered_map is present for $CXX but neither -std=c++0x nor -std=gnu++0x is accepted])   
      fi
      MPI_CXXFLAGS="$MPI_CXXFLAGS -DHAVE_UNORDERED_MAP"
   fi
   CXX="$SAVECXX"
fi

# set correct map type depending on whether unordered_map is present
AC_COMPILE_STDCXX_0X
AX_CXX_HEADER_UNORDERED_MAP
if test "$ax_cv_cxx_unordered_map" = "yes"; then
   if test "$ac_cv_cxx_compile_cxx0x_gxx" = "yes"; then
      CXXFLAGS="$CXXFLAGS -std=gnu++0x";
   elif test "$ac_cv_cxx_compile_cxx0x_cxx" = "yes"; then
      CXXFLAGS="$CXXFLAGS -std=c++0x";
   else
      AC_MSG_ERROR([configure detected that unordered_map is present for $CXX but neither -std=c++0x nor -std=gnu++0x is accepted])   
   fi
   CXXFLAGS="$CXXFLAGS -DHAVE_UNORDERED_MAP"
fi

AC_ARG_ENABLE(static, AC_HELP_STRING([--enable-static], [Build a statically-linked pebil executable in addition to a dynamically-linked executable [yes]]), static_ok=$enableval)
if test "$static_ok" != "no"; then
   PEBIL_STATIC_BUILD="yes"
else
   PEBIL_STATIC_BUILD="no"
fi
AC_SUBST(PEBIL_STATIC_BUILD)

AC_ARG_ENABLE(mpiinit_check, AC_HELP_STRING([--enable-mpiinit-check], [Disable output file printing in inst libraries unless mpi_init is executed [no]]), mpiinit_check_ok=$enableval)
if test "$mpiinit_check_ok" = "yes"; then
   MPI_FLAGS="$MPI_FLAGS -DMPI_INIT_REQUIRED"
fi

AC_SUBST(MPI_FLAGS)
AC_SUBST(MPI_CXXFLAGS)

AC_ARG_WITH(throttler_headers,
            [AS_HELP_STRING([--with-throttler-headers=<path>],[specify path to throttler headers])],
            [throttler_headers=$withval
             AC_CHECK_HEADER([$throttler_headers/throttler.h],
                             [THROTTLER_FLAGS="-DHAVE_THROTTLER_H -I$throttler_headers"],
                             [AC_MSG_ERROR(throttler.h not found using $throttler_headers)])],
            [throttler_headers=no])
AC_SUBST(THROTTLER_FLAGS)

AC_ARG_ENABLE(mic_libs, AC_HELP_STRING([--enable-mic-libs], [Create mic runtime libraries [yes]]), mic_libs_ok=$enableval)
if test "$mic_libs_ok" = "yes"; then
  PEBIL_MIC_LIBS_BUILD="yes"
else
  PEBIL_MIC_LIBS_BUILD="no"
fi
AC_SUBST(PEBIL_MIC_LIBS_BUILD)

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h unistd.h])
AC_CHECK_HEADERS([cpufreq.h],
        CPUFREQ_FLAGS="-DHAVE_CPUFREQ_H")
AC_SUBST(CPUFREQ_FLAGS)

AC_CHECK_HEADERS([adm.h],
        [ADAMANT_FLAGS="-DHAVE_ADAMANT";ADAMANT_LIBS_BUILD="yes"],ADAMANT_LIBS_BUILD="no")
AC_SUBST(ADAMANT_FLAGS)
AC_SUBST(ADAMANT_LIBS_BUILD)

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
                 testapps/apps/Makefile
                 testapps/toys/Makefile
                 instcode/Makefile
                 scripts/blacklist.sh
                 cshrc
                 bashrc])
AC_CONFIG_SUBDIRS([external/udis86-1.7])
AC_CONFIG_SUBDIRS([external/ReuseDistance])
AC_CONFIG_SUBDIRS([external/ReuseDistanceForMIC]) # Probably only wanna do if MIC
AC_OUTPUT
chmod +x scripts/blacklist.sh

exit 0
