#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <msg/msg.h>
#include <net/resolve.h>
#include <net/socket.h>
#include <unix/nonblock.h>
#include "ucspi-proxy.h"

static ipv4addr addr;
static long port;

int tcp_connect(const char* host, const char* portstr, unsigned timeout)
{
  const char* end;
  int fd;
  iopoll_fd pf;

  if (!resolve_ipv4name(host, &addr))
    die3(111, "Could not resolve '", host, "'");
  if ((port = strtol(portstr, (char**)&end, 10)) <= 0 ||
      port >= 0xffff || *end != 0)
    die2(111, "Invalid port number: ", portstr);

  if ((fd = socket_tcp()) == -1)
    warn1sys("Could not create outgoing socket");
  else if (!nonblock_on(fd))
    warn1sys("Could not set flags on new outgoing socket");
  else if (socket_connect4(fd, &addr, port))
    return fd;
  else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
    pf.fd = fd;
    pf.events = IOPOLL_WRITE;
    switch (iopoll_restart(&pf, 1, timeout * 1000)) {
    case 0:
      warn1("Connection timed out");
      errno = ETIMEDOUT;
      break;
    case 1:
      if (socket_connected(fd))
	return fd;
    }
  }
  warn1sys("Connection to server rejected");
  close(fd);
  return -1;
}
