dnl @synopsis AC_C_INT64_T
dnl
dnl Provides a test for the existance of the int64_t type and
dnl defines HAVE_INT64_T if it is found. Adapted from AC_C_LONG_LONG by
dnl Caolan McNamara <caolan@skynet.ie>
dnl
dnl @version $Id: AC_C_INT64_T.m4,v 1.1 2001/10/03 22:14:39 bastiaan Exp $
dnl @author Bastiaan Bakker <bastiaan.bakker@lifeline.nl>
dnl
AC_DEFUN([AC_C_INT64_T],
[AC_CACHE_CHECK(for int64_t, ac_cv_c_int64_t,
[if test "$GCC" = yes; then
  ac_cv_c_int64_t=yes
 else
  AC_TRY_COMPILE(,[int64_t i;], ac_cv_c_int64_t=yes, ac_cv_c_int64_t=no)
 fi
]) 
if test $ac_cv_c_int64_t = yes; then
  AC_DEFINE(HAVE_INT64_T,,[define if the compiler has int64_t])
fi
])

