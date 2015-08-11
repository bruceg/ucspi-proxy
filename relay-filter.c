#include <bglibs/sysdeps.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <bglibs/msg.h>
#include <bglibs/sig.h>
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
    error1sys("Could not fork");
    return;
  case 0:
    execvp(relay_command[0], relay_command);
    exit(1);
  }
}

static void catch_child(int ignored)
{
  waitpid(-1, 0, WNOHANG);
  ignored = 0;
}

static void catch_alarm(int ignored)
{
  if (relay_command == 0)
    return;
  /* Run the relay-ctrl process, and then set it up to re-run */
  if (opt_verbose)
    msg2("Running ", relay_command[0]);
  run_relay_ctrl();
  alarm(relay_rerun_delay);
  ignored = 0;
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

static void setenvb(const char* name, const char* value, size_t len)
{
  char* s;
  size_t nlen = strlen(name);
  if ((s = malloc(len + nlen + 2)) == 0)
    die_oom(111);
  memcpy(s, name, nlen);
  s[nlen++] = '=';
  memcpy(s + nlen, value, len);
  s[nlen + len] = 0;
  if (putenv(s) != 0)
    die_oom(111);
}

/* Export the username through $USER and $DOMAIN */
static void export_client(const char* username)
{
  const char* at;
  if ((at = strrchr(username, '@')) == 0) {
    setenvb("USER", username, strlen(username));
    setenvb("DOMAIN", 0, 0);
  }
  else {
    setenvb("USER", username, at - username);
    ++at;
    setenvb("DOMAIN", at, strlen(at));
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
  if (username)
    export_client(username);
  
  /* Turn off all further filtering, as this IP has already authenticated */
  set_filter(CLIENT_IN, (filter_fn)write_server, 0);
  set_filter(SERVER_FD, (filter_fn)write_client, 0);

  sig_child_catch(catch_child);
  sig_alarm_catch(catch_alarm);
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
