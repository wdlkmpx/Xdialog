include ../config.mk

APP  = Xdialog
# ls *.c | sed 's%\.c%.o%g' | tr '\n' ' ' >> Makefile
OBJS = callbacks.o interface.o main.o support.o

#CPPFLAGS += -I..
CFLAGS += -I..

CFLAGS += $(GTK_CFLAGS)
LIBS   += $(GTK_LIBS)

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(APP) $(LDFLAGS) $(LIBS)

strip: $(APP)
	$(STRIP) $(APP)

distclean: clean

clean:
	rm -f *.o *~ $(APP) $(APP).exe

install: $(APP)
	mkdir -p $(DESTDIR)$(bindir)
	install -c $(APP) $(DESTDIR)$(bindir)

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)
