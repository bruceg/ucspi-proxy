#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "ucspi-proxy.h"

const char program[] = "ucspi-proxy";
const char filter_usage[] = "";

const char filter_connfail_prefix[] = "";
const char filter_connfail_suffix[] = "\r\n";

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
  add_filter(CLIENT_IN, write_server, 0);
  add_filter(SERVER_FD, write_client, 0);
}

void filter_deinit(void)
{
}
