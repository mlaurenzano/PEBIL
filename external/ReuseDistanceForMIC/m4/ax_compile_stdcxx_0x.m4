# AC_COMPILE_STDCXX_OX
AC_DEFUN([AC_COMPILE_STDCXX_0X], [
  AC_CACHE_CHECK(if g++ supports C++0x features without additional flags,
  ac_cv_cxx_compile_cxx0x_native,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([
  #include <unordered_map>],,
  ac_cv_cxx_compile_cxx0x_native=yes, ac_cv_cxx_compile_cxx0x_native=no)
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++0x features with -std=c++0x,
  ac_cv_cxx_compile_cxx0x_cxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=c++0x"
  AC_TRY_COMPILE([
  #include<unordered_map>],,
  ac_cv_cxx_compile_cxx0x_cxx=yes, ac_cv_cxx_compile_cxx0x_cxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++0x features with -std=gnu++0x,
  ac_cv_cxx_compile_cxx0x_gxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=gnu++0x"
  AC_TRY_COMPILE([
  #include <unordered_map>],,
  ac_cv_cxx_compile_cxx0x_gxx=yes, ac_cv_cxx_compile_cxx0x_gxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  if test "$ac_cv_cxx_compile_cxx0x_native" = yes ||
     test "$ac_cv_cxx_compile_cxx0x_cxx" = yes ||
     test "$ac_cv_cxx_compile_cxx0x_gxx" = yes; then
    AC_DEFINE(HAVE_STDCXX_0X,,[Define if g++ supports C++0x features. ])
  fi
])