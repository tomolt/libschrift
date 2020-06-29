# Customize below to fit your system

# compiler and linker
CC = cc
LD = cc
AR = ar
RANLIB = ranlib

# installation paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# X11 API installation paths (needed by sftdemo)
X11INC = /usr/include/X11
X11LIB = /usr/lib/X11
# Uncomment below to build sftdemo on OpenBSD
#X11INC = /usr/X11R6/include
#X11LIB = /usr/X11R6/lib

# flags
CPPFLAGS = -I./
CFLAGS = -Os -std=c99 -pedantic -Wall -Wextra
LDFLAGS = -Os

