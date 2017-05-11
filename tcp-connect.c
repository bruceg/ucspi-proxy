#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bglibs/msg.h>
#include <bglibs/resolve.h>
#include <bglibs/socket.h>
#include <bglibs/unix.h>
#include "ucspi-proxy.h"

static int connect_socket(int fd, const ipv4addr* addr, ipv4port port, unsigned timeout, const ipv4addr* srcaddr)
{
  if (srcaddr != NULL && !socket_bind4(fd, srcaddr, 0))
    warn1sys("Could not bind local address on outgoing socket");
  if (!nonblock_on(fd)) {
    error1sys("Could not set flags on new outgoing socket");
    return 0;
  }
  if (socket_connect4(fd, addr, port))
    return 1;
  if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
    iopoll_fd pf;
    pf.fd = fd;
    pf.events = IOPOLL_WRITE;
    switch (iopoll_restart(&pf, 1, timeout * 1000)) {
    case 0:
      error1("Connection timed out");
      errno = ETIMEDOUT;
      break;
    case 1:
      if (socket_connected(fd))
	return 1;
    }
  }
  error1sys("Connection to server rejected");
  return 0;
}

int tcp_connect(const char* host, const char* portstr, unsigned timeout, const char* srcstr)
{
  struct servent* se;
  const char* end;
  int fd;
  ipv4addr addr;
  long port;
  ipv4addr srcaddr;
  ipv4addr* srcaddrp = NULL;

  if (!resolve_ipv4name(host, &addr))
    die3(111, "Could not resolve '", host, "'");
  if ((se = getservbyname(portstr, "tcp")) != 0)
    port = ntohs(se->s_port);
  else if ((port = strtol(portstr, (char**)&end, 10)) <= 0 ||
	   port >= 0xffff || *end != 0)
    die2(111, "Invalid port number: ", portstr);

  if (srcstr != NULL) {
    ipv4port srcport;
    if (strcmp(srcstr, "auto") == 0) {
      if (!socket_getaddr4(0, &srcaddr, &srcport))
        warn1sys("Could not get address of incoming connection");
      else
        srcaddrp = &srcaddr;
    }
    else {
      const char* p;
      if ((p = ipv4_scan(srcstr, &srcaddr)) == NULL || *p != 0)
        warnf("{Invalid source address \"}s{\"}", srcstr);
      else
        srcaddrp = &srcaddr;
    }
  }

  if ((fd = socket_tcp()) == -1)
    error1sys("Could not create outgoing socket");
  else if (!connect_socket(fd, &addr, port, timeout, srcaddrp)) {
    close(fd);
    fd = -1;
  }
  return fd;
}
