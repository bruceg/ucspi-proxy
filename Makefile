CC = gcc
CFLAGS = -O -W -Wall

LD = $(CC)
LDFLAGS =
LIBS =

ucspi-proxy: ucspi-proxy.o
	$(LD) $(LDFLAGS) -o ucspi-proxy ucspi-proxy.o $(LIBS)

rsync: ucspi-proxy
	rsync -v ucspi-proxy tcp-proxy root@sinclair:/usr/local/bin
