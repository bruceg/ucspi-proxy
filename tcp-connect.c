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

static unsigned long timeout = 0;

int tcp_connect(void)
{
  const char* tmp;
  const char* end;
  int fd;
  iopoll_fd pf;

  if ((tmp = getenv("PROXY_CONNECT_TIMEOUT")) != 0) {
    timeout = strtoul(tmp, (char**)&end, 10);
    if (*end != 0)
      die2(111, "Invalid connect timeout: ", tmp);
  }
  if (timeout == 0) timeout = 30;

  if ((tmp = getenv("PROXY_CONNECT_HOST")) != 0) {
    if (!resolve_ipv4name(tmp, &addr))
      die3(111, "Could not resolve, '", tmp, "'");
  }
  else if ((tmp = getenv("PROXY_CONNECT_ADDR")) != 0) {
    if (!ipv4_parse(tmp, &addr, &end) || *end != 0)
      die3(111, "Could not parse IP address '", tmp, "'");
  }
  else
    die1(111, "No host specified to connect to");
  if ((tmp = getenv("PROXY_CONNECT_PORT")) == 0)
    die1(111, "$PROXY_CONNECT_PORT is not set");
  if ((port = strtol(tmp, (char**)&end, 10)) <= 0 ||
      port > 0xffff || *end != 0)
    die3(111, "Invalid connect port number '", tmp, "'");

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
