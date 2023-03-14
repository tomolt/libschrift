# See LICENSE file for copyright and license details.

.POSIX:

include ../../config.mk

.PHONY: all clean

all: gl-sample

gl-sample: gl-sample.o ../../libschrift.a
	$(LD) $(EXTRAS_LDFLAGS) $@.o -o $@ -L../.. -lglfw -lepoxy -lschrift -lm
gl-sample.o: gl-sample.c ../../schrift.h ../include/utf8_to_utf32.h
	$(CC) -c $(EXTRAS_CFLAGS) $(@:.o=.c) -o $@ $(EXTRAS_CPPFLAGS) -I../..

clean:
	rm -f gl-sample.o gl-sample
