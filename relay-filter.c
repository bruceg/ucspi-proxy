#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <msg/msg.h>
#include "ucspi-proxy.h"

static unsigned relay_rerun_delay;
static const char* client_ip;
static char** relay_command;

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
  if (relay_command == 0)
    return;
  /* Run the relay-ctrl process, and then set it up to re-run */
  if (opt_verbose)
    msg2("Running ", relay_command[0]);
  run_relay_ctrl();
  signal(SIGALRM, catch_alarm);
  alarm(relay_rerun_delay);
}

void relay_init(int argc, char* argv[])
{
  char* tmp;
  client_ip = getenv("TCPREMOTEIP");
  if (argc >= 1) {
    relay_command = argv;
    if ((tmp = getenv("PROXY_RELAY_INTERVAL")) != 0) {
      relay_rerun_delay = strtoul(tmp, &tmp, 10);
      if (*tmp != 0)
	usage("Delay parameter is not a positive number");
    }
    if (relay_rerun_delay == 0)
      relay_rerun_delay = 300;
  }
}

void accept_client(const char* username)
{
  if (opt_verbose) {
    if (username)
      msg5("Accepted relay client ", client_ip, ", username '", username, "'");
    else
      msg3("Accepted relay client ", client_ip, ", username unknown");
  }
  
  /* Turn off all further filtering, as this IP has already authenticated */
  set_filter(CLIENT_IN, (filter_fn)write_server, 0);
  set_filter(SERVER_FD, (filter_fn)write_client, 0);

  catch_alarm(0);
}

void deny_client(const char* username)
{
  if (opt_verbose) {
    if (username)
      msg5("Failed login from ", client_ip, ", username '", username, "'");
    else
      msg3("Failed login from ", client_ip, ", username unknown");
  }
}
