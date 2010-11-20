#
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
AC_LANG_PUSH([Fortran])
ACX_MPI
AC_LANG_POP
AC_LANG_PUSH(C++)
ACX_MPI
AC_LANG_POP()

AC_ARG_ENABLE(static, AC_HELP_STRING([--enable-static], [Use static archives for instrumentation libraries [no]]), static_ok=$enableval)
if test "$static_ok" = "yes"; then
   STATIC_INST_LIB="yes"
fi
AC_SUBST(STATIC_INST_LIB)

AC_CHECK_HEADERS([cpufreq.h], THROTTLE_FLAGS="-DHAVE_CPUFREQ", THROTTLE_FLAGS="")
AC_SUBST(THROTTLE_FLAGS)

blacklist() { readelf -s $1 | grep FUNC |  grep -v UND | awk '{print $8}' | grep -wvi "main\|main_\|main__\|_fini\|_start"; }

AC_ARG_ENABLE(mpi, AC_HELP_STRING([--enable-mpi], [Use mpi compiler when building instrumentation libraries [yes]]), mpi_ok=$enableval)
if test "$mpi_ok" = "no"; then
   MPICC="$CC"
   MPIFC="$FC"
   MPICXX="$CXX"
   MPI_FLAGS=""

   AC_MSG_CHECKING([Fortran function listing (serial)])
   AC_LANG_PUSH(Fortran)
   AC_TRY_RUN([
      program test
      integer ierr
      end
      ],
      [FORTRAN_BLACKLIST=`blacklist conftest$ac_exeext`],
      [FORTRAN_BLACKLIST=`blacklist conftest$ac_exeext`])
   AC_LANG_POP()
   bfuncs=`echo "$FORTRAN_BLACKLIST" | wc | awk '{print $2}'`
   AC_MSG_RESULT($bfuncs functions)
   AC_MSG_CHECKING([C++ function listing (serial)])
   AC_LANG_PUSH(C++)
   AC_TRY_RUN([int main(int argc, char** argv) { return 0; }],
      [CXX_BLACKLIST=`blacklist conftest$ac_exeext`],
      [CXX_BLACKLIST=`blacklist conftest$ac_exeext`])
   AC_LANG_POP()
   bfuncs=`echo "$CXX_BLACKLIST" | wc | awk '{print $2}'`
   AC_MSG_RESULT($bfuncs functions)
   AC_MSG_CHECKING([C function listing (serial)])
   AC_TRY_RUN([int main(int argc, char** argv) { return 0; }], 
      [C_BLACKLIST=`blacklist conftest$ac_exeext`],
      [C_BLACKLIST=`blacklist conftest$ac_exeext`])
   bfuncs=`echo "$C_BLACKLIST" | wc | awk '{print $2}'`
   AC_MSG_RESULT($bfuncs functions)

else
   MPI_FLAGS="-DHAVE_MPI"

   SAVE_CC="$CC"
   SAVE_FC="$FC"
   SAVE_CXX="$CXX"
   CC="$MPICC"
   FC="$MPIFC"
   CXX="$MPICXX"

   AC_MSG_CHECKING([Fortran function listing (mpi)])
   AC_LANG_PUSH(Fortran)
   AC_TRY_RUN([
      program test
      include 'mpif.h'
      integer ierr
      call MPI_Init(ierr)
      call MPI_Finalize (ierr)
      end
      ],
      [FORTRAN_BLACKLIST=`blacklist conftest$ac_exeext`],
      [FORTRAN_BLACKLIST=`blacklist conftest$ac_exeext`])
   AC_LANG_POP()
   bfuncs=`echo "$FORTRAN_BLACKLIST" | wc | awk '{print $2}'`
   AC_MSG_RESULT($bfuncs functions)
   AC_MSG_CHECKING([C++ function listing (mpi)])
   AC_LANG_PUSH(C++)
   AC_TRY_RUN([#include <mpi.h>
      using namespace std; int main(int argc, char** argv) { MPI::Init(argc, argv); MPI::Finalize(); return 0; }],
      [CXX_BLACKLIST=`blacklist conftest$ac_exeext`],
      [CXX_BLACKLIST=`blacklist conftest$ac_exeext`])
   AC_LANG_POP()
   bfuncs=`echo "$CXX_BLACKLIST" | wc | awk '{print $2}'`
   AC_MSG_RESULT($bfuncs functions)
   AC_MSG_CHECKING([C function listing (mpi)])
   AC_TRY_RUN([#include <mpi.h>
      int main(int argc, char** argv) { MPI_Init(&argc, &argv); MPI_Finalize(); return 0; }], 
      [C_BLACKLIST=`blacklist conftest$ac_exeext`],
      [C_BLACKLIST=`blacklist conftest$ac_exeext`])
   bfuncs=`echo "$C_BLACKLIST" | wc | awk '{print $2}'`
   AC_MSG_RESULT($bfuncs functions)

   CC="$SAVE_CC"
   FC="$SAVE_FC"
   CXX="$SAVE_CXX"
fi
AC_SUBST(FORTRAN_BLACKLIST)
AC_SUBST(CXX_BLACKLIST)
AC_SUBST(C_BLACKLIST)

AC_ARG_ENABLE(mpiinit_check, AC_HELP_STRING([--enable-mpiinit-check], [Disable output file printing in inst libraries unless mpi_init is executed [no]]), mpiinit_check_ok=$enableval)
if test "$mpiinit_check_ok" = "yes"; then
   MPI_FLAGS="$MPI_FLAGS -DMPI_INIT_REQUIRED"
fi

AC_SUBST(MPI_FLAGS)

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
                 instcode/Makefile
                 scripts/inputlist/local.func])
AC_CONFIG_SUBDIRS([external/udis86-1.7])
AC_OUTPUT

sort -u scripts/inputlist/local.func > scripts/inputlist/autogen-system.func
exit 0
