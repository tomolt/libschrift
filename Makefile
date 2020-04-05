# See LICENSE file for copyright and license details.

.POSIX:

include config.mk

.PHONY: all clean install uninstall

all: libschrift.a sftdemo

libschrift.a: schrift.o
	$(AR) rc $@ $^$>
	$(RANLIB) $@

sftdemo: sftdemo.o libschrift.a
	$(LD) $(LDFLAGS) $< -o $@ -L. -lschrift

sftdemo.o schrift.o: schrift.h
sftdemo.o: arg.h

clean:
	rm -f *.o
	rm -f libschrift.a
	rm -f sftdemo

install: libschrift.a schrift.h
	mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	cp -f libschrift.a "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p "$(DESTDIR)$(PREFIX)/include"
	cp -f schrift.h "$(DESTDIR)$(PREFIX)/include"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/lib/libschrift.a"
	rm -f "$(DESTDIR)$(PREFIX)/include/schrift.h"

