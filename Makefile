PACKAGE = ucspi-proxy
VERSION = 0.91

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

SOURCES = ucspi-proxy.c null-filter.c ucspi-proxy.h ucspi-proxy.1 \
	ftp-filter.c log-filter.c pop3-relay-filter.c
PROGS = ucspi-proxy ucspi-proxy-log \
	ucspi-proxy-pop3-relay
	# ucspi-proxy-ftp
SCRIPTS = tcp-proxy ftp-proxy log-proxy \
	pop3-relay-proxy

all: $(PROGS)

ucspi-proxy: ucspi-proxy.o null-filter.o
	$(LD) $(LDFLAGS) -o $@ ucspi-proxy.o null-filter.o $(LIBS)

ucspi-proxy-ftp: ucspi-proxy.o ftp-filter.o
	$(LD) $(LDFLAGS) -o $@ ucspi-proxy.o ftp-filter.o $(LIBS)

ucspi-proxy-log: ucspi-proxy.o log-filter.o
	$(LD) $(LDFLAGS) -o $@ ucspi-proxy.o log-filter.o $(LIBS)

ucspi-proxy-pop3-relay: ucspi-proxy.o pop3-relay-filter.o
	$(LD) $(LDFLAGS) -o $@ ucspi-proxy.o pop3-relay-filter.o $(LIBS)

install:
	$(install) -d $(bindir)
	$(install) -m 755 $(PROGS) $(SCRIPTS) $(bindir)

	$(install) -d $(man1dir)
	$(install) -m 644 ucspi-proxy.1 $(man1dir)

clean:
	$(RM) *.o $(PROGS)
