#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static unsigned relay_rerun_delay;
static const char* client_ip;
static char* relay_command[3] = { 0, "/bin/true", 0 };

static void run_relay_ctrl(void)
{
  /* Run relay-ctrl-allow to allow the client's IP to relay */
  const pid_t p = fork();
  switch (p) {
  case -1:
    return;
  case 0:
    execvp(relay_command[0], relay_command);
    exit(1);
  default:
    waitpid(p, 0, 0);
  }
}

static void catch_alarm(int ignored)
{
  if (relay_command[0] == 0)
    return;
  /* Run the relay-ctrl process, and then set it up to re-run */
  fprintf(stderr, "%s: Running relay-ctrl-allow\n", program);
  run_relay_ctrl();
  signal(SIGALRM, catch_alarm);
  alarm(relay_rerun_delay);
}

void relay_init(void)
{
  char* tmp;
  client_ip = getenv("TCPREMOTEIP");
  relay_command[0] = getenv("PROXY_RELAY_COMMAND");
  if ((tmp = getenv("PROXY_RELAY_ARG")) != 0)
    relay_command[1] = tmp;
  if ((tmp = getenv("PROXY_RELAY_INTERVAL")) != 0) {
    relay_rerun_delay = strtoul(tmp, &tmp, 10);
    if (*tmp != 0)
      usage("Delay parameter is not a positive number");
  }
  if (relay_rerun_delay == 0)
    relay_rerun_delay = 300;
}

void accept_client(const char* username)
{
  if (username)
    fprintf(stderr, "%s: Accepted relay client %s, username '%s'\n",
	    program, client_ip, username);
  else
    fprintf(stderr, "%s: Accepted relay client %s, unknown username\n",
	    program, client_ip);
  
  /* Turn off all further filtering, as this IP has already authenticated */
  del_filter(CLIENT_IN);
  del_filter(SERVER_FD);
  add_filter(CLIENT_IN, (filter_fn)write_server, 0);
  add_filter(SERVER_FD, (filter_fn)write_client, 0);

  catch_alarm(0);
}

void deny_client(const char* username)
{
  if (username)
    fprintf(stderr, "%s: Failed login from %s, username '%s'\n",
	    program, client_ip, username);
  else
    fprintf(stderr, "%s: Failed login from %s, unknown username\n",
	    program, client_ip);
}
