# See LICENSE file for copyright and license details.

.POSIX:

include ../../config.mk

.PHONY: all clean

all: x11-sample

x11-sample: x11-sample.o libschrift.a
	$(LD) $(EXTRAS_LDFLAGS) $@.o -o $@ -L$(X11LIB) -L../.. -lX11 -lXrender -lschrift -lm
x11-sample.o: x11-sample.c schrift.h ../include/utf8_to_utf32.h
	$(CC) -c $(EXTRAS_CFLAGS) $(@:.o=.c) -o $@ $(EXTRAS_CPPFLAGS) -I$(X11INC) -I../include

clean:
	rm -f x11-sample.o x11-sample
