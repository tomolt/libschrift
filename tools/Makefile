# See LICENSE file for copyright and license details.

.POSIX:

include ../config.mk

.PHONY: all clean

all: stress

stress: stress.o ../libschrift.a
	$(LD) $(EXTRAS_LDFLAGS) $@.o -o $@ -L.. -lschrift -lm
stress.o: stress.c ../schrift.h arg.h
	$(CC) -c $(EXTRAS_CFLAGS) $(@:.o=.c) -o $@ $(EXTRAS_CPPFLAGS) -I..

clean:
	rm -f stress.o stress
