#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

const char* filter_name = "ucspi-proxy-log";

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
}

static void show(char* data, ssize_t size, char prefix)
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

void filter_client_data(char** data, ssize_t* size)
{
  show(*data, *size, '>');
}

void filter_server_data(char** data, ssize_t* size)
{
  show(*data, *size, '<');
}

void filter_deinit(void)
{
}
