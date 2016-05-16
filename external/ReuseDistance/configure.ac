#
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([ReuseDistance], [0.01], [michaell@sdsc.edu])
AC_CONFIG_SRCDIR([ReuseDistance.cpp])
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/ax_cxx_header_unordered_map.m4])
m4_include([m4/ax_compile_stdcxx_0x.m4])


# Checks for programs.
AC_PROG_CXX

# set correct map type depending on whether unordered_map is present
AC_COMPILE_STDCXX_0X
AX_CXX_HEADER_UNORDERED_MAP
if test "$ax_cv_cxx_unordered_map" = "yes"; then
   if test "$ac_cv_cxx_compile_cxx0x_cxx" = "yes"; then
      CXXFLAGS="$CXXFLAGS -std=c++0x";
   elif test "$ac_cv_cxx_compile_cxx0x_gxx" = "yes"; then
      CXXFLAGS="$CXXFLAGS -std=gnu++0x";
   elif test "$ac_cv_cxx_compile_cxx0x_native" = "yes"; then
      x=1
   else
      AC_MSG_ERROR([configure detected that unordered_map is present but neither -std=c++0x, -std=gnu++0x nor native mode is recognized])   
   fi
   CXXFLAGS="$CXXFLAGS -DHAVE_UNORDERED_MAP"
else
   AC_MSG_WARN([Missing support for c++0x unordered_map. You can build a functional tool without it but it will perform poorly.]) 
fi

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h unistd.h])

AC_CONFIG_FILES([Makefile
                 test/Makefile
                 docs/Makefile
                 env/cshrc
                 env/bashrc])


PACKAGE=ReuseDistance
AC_SUBST(PACKAGE)

AC_OUTPUT
