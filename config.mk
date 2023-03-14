# Customize below to fit your system

# compiler and linker
CC = cc
LD = cc
AR = ar
RANLIB = ranlib

# installation paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# compiler flags for libschrift
CPPFLAGS =
CFLAGS   = -Os -std=c99 -pedantic -Wall -Wextra -Wconversion
LDFLAGS  = -Os

# compiler flags for the samples & tools
EXTRAS_CPPFLAGS =
EXTRAS_CFLAGS   = -g -std=c99 -pedantic -Wall -Wextra
EXTRAS_LDFLAGS  = -g

# X11 API installation paths (needed by the demo)
X11INC = /usr/include/X11
X11LIB = /usr/lib/X11
# Uncomment below to build the demo on OpenBSD
#X11INC = /usr/X11R6/include
#X11LIB = /usr/X11R6/lib

