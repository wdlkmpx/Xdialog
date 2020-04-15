dnl Process this file with autoconf to produce a configure script.

AC_INIT([Xdialog],[2.3.1])
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign silent-rules])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_PROG_AR
LT_INIT

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(errno.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_FUNCS(strstr memchr memmove setlocale)

PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.0],[AC_MSG_NOTICE([Using GTK2])])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

dnl Set PACKAGE_DATA_DIR in config.h.
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}", [ ])
  else
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}", [ ])
  fi
else
  AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${datadir}/${PACKAGE}", [ ])
fi

dnl Set PACKAGE_SOURCE_DIR in config.h.
packagesrcdir=`cd $srcdir && pwd`
AC_DEFINE_UNQUOTED(PACKAGE_SOURCE_DIR, "${packagesrcdir}", [ ])

case "$CC" in
  gcc*) CPPFLAGS="$CPPFLAGS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers" ;;
  *) ;;
esac

AC_ARG_WITH(french-labels,
  [  --with-french-labels    Enables french labels in menus],[
  CPPFLAGS="$CPPFLAGS -DFRENCH"
  AC_MSG_RESULT(Compiling with french labels)
  ])

AC_ARG_WITH(fixed-font,
  [  --with-fixed-font=font_name
                          Sets the fixed font to be used],[
  if test x$withval = xyes; then
    AC_MSG_ERROR([Usage is: --with-fixed-font=font_name])
  else
    CPPFLAGS="$CPPFLAGS -DFIXED_FONT=\\\"$withval\\\""
    AC_MSG_RESULT(Using \"$withval\" fixed font)
  fi
  ])

AC_ARG_WITH(scanf-calls,
  [  --with-scanf-calls      Use scanf calls instead of non-blocking reads],[
  CPPFLAGS="$CPPFLAGS -DUSE_SCANF"
  AC_MSG_RESULT(Using scanf calls instead of non-blocking read calls)
  ])

AC_ARG_WITH(print-command,
  [  --with-print-command=cmd
                          Sets the printer command to be used],[
  if test x$withval = xyes; then
    AC_MSG_ERROR([Usage is: --with-printer-command=cmd])
  else
    CPPFLAGS="$CPPFLAGS -DPRINTER_CMD=\\\"$withval\\\""
    AC_MSG_RESULT(Compiling with printer command: \"$withval\")
  fi
  ])

AC_ARG_WITH(print-option,
  [  --with-print-option=option
                          Sets the printer command option to be used],[
  if test x$withval = xyes; then
    AC_MSG_ERROR([Usage is: --with-printer-option=option])
  else
    CPPFLAGS="$CPPFLAGS -DPRINTER_CMD_OPTION=\\\"$withval\\\""
    AC_MSG_RESULT(Compiling with printer command option: \"$withval\")
  fi
  ])

AC_OUTPUT(Makefile
src/Makefile
doc/Makefile)
