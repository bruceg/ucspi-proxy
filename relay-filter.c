#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ucspi-proxy.h"

void accept_ip(const char* client_ip)
{
  pid_t pid;
  
  /* Turn off all further filtering, as this IP has already authenticated */
  del_filter(CLIENT_IN);
  del_filter(SERVER_IN);
  add_filter(CLIENT_IN, write_server, 0);
  add_filter(SERVER_IN, write_client, 0);

  /* Run relay-ctrl-allow to allow the client's IP to relay */
  pid = fork();
  switch(pid) {
  case -1:
    return;
  case 0:
    setenv("TCPREMOTEIP", client_ip, 1);
    execl("/usr/sbin/relay-ctrl-allow",
	  "/usr/sbin/relay-ctrl-allow", "/bin/true", 0);
    exit(1);
  default:
    waitpid(pid, 0, 0);
  }
}
