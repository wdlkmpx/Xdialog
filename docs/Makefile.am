## Process this file with automake to produce Makefile.in

docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)

CLEANFILES = Xdialog.1

doc_DATA = $(srcdir)/*.html $(srcdir)/*.png

man_MANS = Xdialog.1

Xdialog.1: Xdialog.man
	sed -e 's/%%version%%/$(VERSION)/' \
	    -e 's:%%bindir%%:$(bindir):' \
	    -e 's:%%mandir%%:$(mandir):' \
	    -e 's:%%datadir%%:$(datadir):' $(srcdir)/Xdialog.man >Xdialog.1
