#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static void show(const char* data, ssize_t size, char prefix)
{
  ssize_t i;
  static char buf[BUFSIZE+4];
  char* ptr = buf;
  *ptr++ = prefix;
  *ptr++ = ' ';
  for(i = 0; i < size; i++)
    *ptr++ = data[i];
  if(data[size-1] != '\n')
    *ptr++ = '+';
  *ptr++ = '\n';
  write(2, buf, ptr-buf);
}

static void filter_client_data(char* data, ssize_t size)
{
  show(data, size, '>');
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  show(data, size, '<');
  write_client(data, size);
}

const char program[] = "ucspi-proxy-log";
const char filter_usage[] = "";

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
