#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

extern void accept_client(void);
extern void deny_client(void);
extern void relay_init(int argc, char** argv);

static bool saw_command = 0;

static void filter_client_data(char* data, ssize_t size)
{
  if(!strncasecmp(data, "PASS ", 5) ||
     !strncasecmp(data, "AUTH ", 5))
    saw_command = 1;
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if(saw_command) {
    if(!strncasecmp(data, "+OK ", 4)) {
      accept_client();
      saw_command = 0;
    }
    else if(!strncasecmp(data, "-ERR ", 5)) {
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
