PACKAGE = ucspi-proxy
VERSION = 0.90

CC = gcc
CFLAGS = -O -g -W -Wall

LD = $(CC)
LDFLAGS = -g
LIBS =

prefix = /usr
install_prefix =
bindir = $(install_prefix)$(prefix)/bin
mandir = $(install_prefix)$(prefix)/man
man1dir = $(mandir)/man1

install = /usr/bin/install

PROGS = ucspi-proxy
SOURCES = ucspi-proxy.c null-filter.c ucspi-proxy.h ucspi-proxy.1
SCRIPTS = tcp-proxy

all: $(PROGS)

ucspi-proxy: ucspi-proxy.o null-filter.o
	$(LD) $(LDFLAGS) -o $@ ucspi-proxy.o null-filter.o $(LIBS)

install:
	$(install) -d $(bindir)
	$(install) -m 755 $(PROGS) $(bindir)

	$(install) -d $(man1dir)
	$(install) -m 644 ucspi-proxy.1 $(man1dir)

clean:
	$(RM) *.o $(progs)
