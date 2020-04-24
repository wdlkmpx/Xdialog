## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign no-dependencies

SUBDIRS = src docs

CONFIG_CLEAN_FILES = ltmain.sh ltconfig autom4te.cache/*

docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)/samples
doc_DATA = $(srcdir)/samples/*

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath
