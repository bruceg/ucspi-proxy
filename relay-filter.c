#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static unsigned relay_rerun_delay;
static const char* client_ip;
static char** relay_command;

static void run_relay_ctrl(void)
{
  /* Run relay-ctrl-allow to allow the client's IP to relay */
  pid_t pid = fork();
  switch(pid) {
  case -1:
    return;
  case 0:
    setenv("TCPREMOTEIP", client_ip, 1);
    execvp(relay_command[0], relay_command);
    exit(1);
  default:
    waitpid(pid, 0, 0);
  }
}

static void catch_alarm(int ignored)
{
  /* Run the relay-ctrl process, and then set it up to re-run */
  fprintf(stderr, "%s: Running relay-ctrl-allow\n", filter_name);
  run_relay_ctrl();
  signal(SIGALRM, catch_alarm);
  alarm(relay_rerun_delay);
}

void relay_init(int argc, char** argv)
{
  char* tmp;
  if(argc < 3)
    usage("Incorrect usage.");
  client_ip = argv[0];
  relay_rerun_delay = strtoul(argv[1], &tmp, 10);
  if(!relay_rerun_delay || *tmp)
    usage("Delay parameter is not a positive number");
  relay_command = argv+2;
}

void accept_client(void)
{
  fprintf(stderr, "%s: Accepted relay client %s\n", filter_name, client_ip);

  /* Turn off all further filtering, as this IP has already authenticated */
  del_filter(CLIENT_IN);
  del_filter(SERVER_IN);
  add_filter(CLIENT_IN, write_server, 0);
  add_filter(SERVER_IN, write_client, 0);

  catch_alarm(0);
}

void deny_client(void)
{
  fprintf(stderr, "%s: Failed login from %s\n", filter_name, client_ip);
}
