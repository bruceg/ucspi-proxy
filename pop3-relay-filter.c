#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

extern void accept_client(const char* username);
extern void deny_client(void);
extern void relay_init(int argc, char** argv);
extern char* base64decode(char* data, unsigned long size);

static bool saw_command = 0;
static bool saw_auth = 0;
static char* username = 0;

static void filter_client_data(char* data, ssize_t size)
{
  char* ptr;
  if (!strncasecmp(data, "PASS ", 5))
    saw_command = 1;
  else if (!strncasecmp(data, "AUTH ", 5))
    saw_auth = saw_command = 1;
  else if (!strncasecmp(data, "USER ", 5)) {
    ptr = data + 5;
    while (isspace(*ptr))
      ++ptr;
    ptr = username = strdup(ptr);
    while (!isspace(*ptr))
      ++ptr;
    *ptr = 0;
  }
  else if (saw_auth) {
    ptr = base64decode(data, size);
    if (ptr) {
      username = ptr;
      while (!isspace(*ptr)) ++ptr;
      *ptr = 0;
    }
    saw_auth = 0;
  }
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if (saw_command) {
    if (!strncasecmp(data, "+OK ", 4)) {
      accept_client(username);
      saw_command = 0;
    }
    else if (!strncasecmp(data, "-ERR ", 5)) {
      deny_client();
      saw_command = 0;
    }
  }
  write_client(data, size);
}

const char* filter_name = "ucspi-proxy-pop3-relay";
const char* filter_usage = "client-ip";

void filter_init(int argc, char** argv)
{
  relay_init(argc, argv);
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_IN, filter_server_data, 0);
}

void filter_deinit(void)
{
}
