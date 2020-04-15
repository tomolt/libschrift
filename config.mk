# Customize below to fit your system

# compiler and linker
CC = cc
LD = cc
AR = ar
RANLIB = ranlib

X11INC = /usr/include/X11
X11LIB = /usr/lib/X11

# flags
CPPFLAGS = -I./
CFLAGS = -g -Og -std=c99 -pedantic -Wall -Wextra
LDFLAGS = -g -Og -lm

# installation paths
PREFIX = /usr/local

