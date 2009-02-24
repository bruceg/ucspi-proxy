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
  set_filter(CLIENT_IN, (filter_fn)write_server, 0);
  set_filter(SERVER_FD, (filter_fn)write_client, 0);
  (void)argv;
}

void filter_deinit(void)
{
}
