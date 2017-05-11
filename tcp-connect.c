#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <bglibs/msg.h>
#include <bglibs/resolve.h>
#include <bglibs/socket.h>
#include <bglibs/unix.h>
#include "ucspi-proxy.h"

static int connect_socket(int fd, const ipv4addr* addr, ipv4port port, unsigned timeout)
{
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

int tcp_connect(const char* host, const char* portstr, unsigned timeout)
{
  struct servent* se;
  const char* end;
  int fd;
  ipv4addr addr;
  long port;


  if (!resolve_ipv4name(host, &addr))
    die3(111, "Could not resolve '", host, "'");
  if ((se = getservbyname(portstr, "tcp")) != 0)
    port = ntohs(se->s_port);
  else if ((port = strtol(portstr, (char**)&end, 10)) <= 0 ||
	   port >= 0xffff || *end != 0)
    die2(111, "Invalid port number: ", portstr);

  if ((fd = socket_tcp()) == -1)
    error1sys("Could not create outgoing socket");
  else if (!connect_socket(fd, &addr, port, timeout)) {
    close(fd);
    fd = -1;
  }
  return fd;
}
