#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ucspi-proxy.h"

extern const char* client_ip;

static void run_relay_ctrl(void)
{
  /* Run relay-ctrl-allow to allow the client's IP to relay */
  pid_t pid = fork();
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

static void catch_alarm(int ignored)
{
  /* Run the relay-ctrl process, and then set it up to re-run in 5 minutes */
  run_relay_ctrl();
  signal(SIGALRM, catch_alarm);
  alarm(5*60);
}

void accept_client(void)
{
  /* Turn off all further filtering, as this IP has already authenticated */
  del_filter(CLIENT_IN);
  del_filter(SERVER_IN);
  add_filter(CLIENT_IN, write_server, 0);
  add_filter(SERVER_IN, write_client, 0);

  catch_alarm(0);
}
