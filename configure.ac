dnl Process this file with autoconf to produce a configure script.

AC_INIT([Xdialog],[2.3.1])
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign silent-rules])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_PROG_AR
LT_INIT

AC_PROG_CC
AC_CHECK_FUNCS(getopt_long_only)

AC_ARG_ENABLE(gtk3,
AS_HELP_STRING([--enable-gtk3],[enable to use gtk-3.0 instead of gtk-2.0]),
[case "${enableval}" in
  yes)  enable_gtk3=yes ;;
  no)   enable_gtk3=no ;;
esac],[])

if test "x$enable_gtk3" = "xyes" ; then
  gtk_modules="gtk+-3.0 >= 3.0.0"
else
  gtk_modules="gtk+-2.0 >= 2.18.0"
fi
PKG_CHECK_MODULES(GTK, [$gtk_modules])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_ARG_ENABLE(gtk_warnings,
	[  --enable-gtk-warnings   So you can see what is deprecated in GTK2...],
	gtk_warnings=$enableval,
	gtk_warnings='no')

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

if test x"$gtk_warnings" = x"yes"; then
   CPPFLAGS="$CPPFLAGS -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_DEPRECATED -DGTK_DISABLE_SINGLE_INCLUDES"
fi

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
lib/Makefile
src/Makefile
doc/Makefile)
