NAME = dwms
VERSION = 1.2

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/include/X11
X11LIB = /usr/lib/X11

# flags
CPPFLAGS = -I${X11INC} -D_DEFAULT_SOURCE
CFLAGS = -std=c99 -pedantic -Wall -Os -march=native -pipe ${CPPFLAGS}
#CFLAGS = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
LDFLAGS = -s -L${X11LIB} -lX11 -lasound

# compiler and linker
CC = cc

