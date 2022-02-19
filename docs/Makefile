
include ../config.mk

Xdialog.1: Xdialog.man
	sed -e 's/%%version%%/$(VERSION)/' \
	    -e 's:%%bindir%%:$(bindir):' \
	    -e 's:%%mandir%%:$(mandir):' \
	    -e 's:%%datadir%%:$(datadir):' $(srcdir)/Xdialog.man >Xdialog.1

all: Xdialog.1

strip:

install:
	mkdir -p $(DESTDIR)$(datadir)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL_DATA) *.html $(DESTDIR)$(datadir)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL_DATA) *.png $(DESTDIR)$(datadir)/doc/$(PACKAGE)-$(VERSION)
	mkdir -p $(DESTDIR)$(mandir)/man1
	$(INSTALL_DATA) Xdialog.1 $(DESTDIR)$(mandir)/man1

install-strip: install

uninstall:
	rm -f $(DESTDIR)$(mandir)/man1/Xdialog.1
	rm -rf $(DESTDIR)$(datadir)/doc/$(PACKAGE)-$(VERSION)

clean:

distclean:


