# See LICENSE file for copyright and license details.

.POSIX:

include config.mk

VERSION=0.10.2

.PHONY: all clean install uninstall dist

all: libschrift.a libschrift.pc

libschrift.a: schrift.o
	$(AR) rc $@ schrift.o
	$(RANLIB) $@
schrift.o: schrift.h

libschrift.pc: libschrift.pc.in
	@sed 's,@prefix@,$(PREFIX),;s,@version@,$(VERSION),' libschrift.pc.in > $@

clean:
	rm -f schrift.o
	rm -f libschrift.a
	rm -f libschrift.pc

install: libschrift.a libschrift.pc schrift.h schrift.3
	# libschrift.a
	mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	cp -f libschrift.a "$(DESTDIR)$(PREFIX)/lib"
	chmod 644 "$(DESTDIR)$(PREFIX)/lib/libschrift.a"
	# libschrift.pc
	mkdir -p "$(DESTDIR)$(PREFIX)/lib/pkgconfig"
	cp -f libschrift.pc "$(DESTDIR)$(PREFIX)/lib/pkgconfig"
	chmod 644 "$(DESTDIR)$(PREFIX)/lib/pkgconfig/libschrift.pc"
	# schrift.h
	mkdir -p "$(DESTDIR)$(PREFIX)/include"
	cp -f schrift.h "$(DESTDIR)$(PREFIX)/include"
	chmod 644 "$(DESTDIR)$(PREFIX)/include/schrift.h"
	# schrift.3
	mkdir -p "$(DESTDIR)$(MANPREFIX)/man3"
	cp schrift.3 "$(DESTDIR)$(MANPREFIX)/man3"
	chmod 644 "$(DESTDIR)$(MANPREFIX)/man3/schrift.3"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/lib/libschrift.a"
	rm -f "$(DESTDIR)$(PREFIX)/lib/pkgconfig/libschrift.pc"
	rm -f "$(DESTDIR)$(PREFIX)/include/schrift.h"
	rm -f "$(DESTDIR)$(MANPREFIX)/man3/schrift.3"

dist:
	rm -rf "schrift-$(VERSION)"
	mkdir -p "schrift-$(VERSION)"
	cp -R README.md LICENSE CHANGELOG.md TODO.md schrift.3 \
		Makefile config.mk libschrift.pc.in \
		schrift.c schrift.h demo.c stress.c \
		resources/ samples/ tools/ \
		"schrift-$(VERSION)"
	tar -cf - "schrift-$(VERSION)" | gzip -c > "schrift-$(VERSION).tar.gz"
	rm -rf "schrift-$(VERSION)"

