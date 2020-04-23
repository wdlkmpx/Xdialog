## Process this file with automake to produce Makefile.in

docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)

CLEANFILES = Xdialog.1

doc_DATA = authors.html \
box.html \
common.html \
compatibility.html \
faq.html \
gtk.html \
index.html \
intro.html \
special.html \
syntax.html \
transient.html \
$(srcdir)/*.png

man_MANS = Xdialog.1

Xdialog.1: Xdialog.man
	sed -e 's/%%version%%/$(VERSION)/' \
	    -e 's:%%bindir%%:$(bindir):' \
	    -e 's:%%mandir%%:$(mandir):' \
	    -e 's:%%datadir%%:$(datadir):' $(srcdir)/Xdialog.man >Xdialog.1
