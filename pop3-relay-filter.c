#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

extern void accept_ip(const char* client_ip);

static bool saw_pass = 0;
static const char* client_ip = 0;

static void filter_client_data(char* data, ssize_t size)
{
  if(!strncasecmp(data, "PASS ", 5))
    saw_pass = 1;
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if(saw_pass) {
    if(!strncasecmp(data, "+OK ", 4))
      accept_ip(client_ip);
    saw_pass = 0;
  }
  write_client(data, size);
}

const char* filter_name = "ucspi-proxy-pop3-relay";
const char* filter_usage = "client-ip";

void filter_init(int argc, char** argv)
{
  if(argc != 1)
    usage("Incorrect usage.");
  client_ip = argv[0];
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_IN, filter_server_data, 0);
}

void filter_deinit(void)
{
}
