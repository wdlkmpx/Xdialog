## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign no-dependencies

localedir = $(datadir)/locale
AM_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" -I. -I../intl -I../lib \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@GTK_CFLAGS@

bin_PROGRAMS = Xdialog

Xdialog_SOURCES = \
	main.c \
	interface.c interface.h \
	callbacks.c callbacks.h \
	support.c support.h

#LDADD = @LIBINTL@
Xdialog_LDADD = $(LDADD) @GTK_LIBS@
