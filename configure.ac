#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([gss-token], [1.1], [elric@imrryr.org])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign 1.10.1])
# AM_MAINTAINER_MODE

AC_CONFIG_MACRO_DIR([cf])

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP

AC_ARG_WITH([gss],
	    [AC_HELP_STRING([--with-gss=<DIR>],
			    [choose the GSSAPI implementation])],
	    [gss=$withval],)

if test -d ${gss}; then
	CPPFLAGS="$CPPFLAGS -I$gss/include"
	LDFLAGS="$LDFLAGS -L$gss/lib -Wl,-rpath=$gss/lib"
fi

AC_ARG_WITH([gss-dir],
	    [AC_HELP_STRING([--with-gss=<DIR>],
			    [choose the GSSAPI implementation])],
	    [gssdir=$withval],)

if test -d ${gssdir}; then
	CPPFLAGS="$CPPFLAGS -I$gssdir/include"
	LDFLAGS="$LDFLAGS -L$gssdir/lib -Wl,-rpath=$gssdir/lib"
fi

AC_CHECK_HEADERS([gssapi/gssapi_krb5.h gssapi/gssapi.h gssapi.h], break,,)
AC_CHECK_HEADERS([krb5/krb5.h krb5.h], break,,)

AC_SEARCH_LIBS(krb5_init_context, [krb5],,,)
AC_SEARCH_LIBS(gss_init_sec_context, [gssapi gssapi_krb5],,,)
AC_CHECK_FUNCS([gss_store_cred_into],,)

#
# We only enable warnings if --enable-developer is specified.

AC_ARG_ENABLE(developer,
	AS_HELP_STRING([--enable-developer],
			[enable developer warnings and experimental code]))
AM_CONDITIONAL(DEVELOPER_MODE, test "X$enable_developer" = Xyes)

if test "X$enable_developer" = Xyes; then
	WFLAGS="-Werror"
	WFLAGS="$WFLAGS -Wall"
	WFLAGS="$WFLAGS -Wextra"
	WFLAGS="$WFLAGS -Wstrict-prototypes"
	WFLAGS="$WFLAGS -Wmissing-prototypes"
	WFLAGS="$WFLAGS -Wpointer-arith"
	WFLAGS="$WFLAGS -Wno-traditional"
	WFLAGS="$WFLAGS -Wreturn-type"
	WFLAGS="$WFLAGS -Wswitch"
	WFLAGS="$WFLAGS -Wno-shadow"
	WFLAGS="$WFLAGS -Wcast-qual"
	WFLAGS="$WFLAGS -Wwrite-strings"
	WFLAGS="$WFLAGS -Wno-unused-parameter"
	WFLAGS="$WFLAGS -Wsign-compare"
fi

AC_SUBST(WFLAGS)dnl

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FORK

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
