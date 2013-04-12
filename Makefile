#
# sdvt
#

PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man
CFLAGS += -Wall
#CPPFLAGS += -DPREFIX=\"$(PREFIX)\"
#CC = cc

PKG_MODULES := vte-2.90 gtk+-3.0
PKG_CFLAGS  := $(shell pkg-config --cflags $(PKG_MODULES))
PKG_LDLIBS  := $(shell pkg-config --libs   $(PKG_MODULES))

all: sdvt sdvt.1

sdvt: CFLAGS += $(PKG_CFLAGS)
sdvt: LDLIBS += $(PKG_LDLIBS)
sdvt: sdvt.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

sdvt.1: sdvt.rst
	rst2man $< $@

clean:
	$(RM) sdvt sdvt.o

install: all
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -t $(DESTDIR)$(PREFIX)/bin sdvt
	install -m 755 -d $(DESTDIR)$(MANPREFIX)/man1
	install -m 644 -t $(DESTDIR)$(MANPREFIX)/man1 sdvt.1
	
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/sdvt
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/sdvt.1

ifeq ($(origin TAG),command line)
VERSION := $(TAG)
else
VERSION := $(shell git tag 2> /dev/null | tail -1)
endif

dist:
ifeq ($(strip $(VERSION)),)
	@echo "ERROR: Either Git is not installed, or no tags were found"
else
	git archive --prefix=sdvt-$(VERSION)/ $(VERSION) | xz -c > sdvt-$(VERSION).tar.xz
endif

.PHONY: clean install uninstall dist