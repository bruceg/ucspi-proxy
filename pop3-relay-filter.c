#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static bool saw_pass = 0;

static void accept_ip(void)
{
  write(2, "Login\n", 6);
}

static void filter_client_data(char* data, ssize_t size)
{
  if(!strncasecmp(data, "PASS ", size))
    saw_pass = 1;
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if(saw_pass) {
    if(strncasecmp(data, "OK ", size))
      accept_ip();
    saw_pass = 0;
  }
  write_client(data, size);
}

const char* filter_name = "ucspi-proxy-log";

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_IN, filter_server_data, 0);
}

void filter_deinit(void)
{
}
