#include <sys/types.h>
#include "ucspi-proxy.h"

const char* filter_name = "ucspi-proxy";

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
}

void filter_client_data(char** data, ssize_t* size)
{
}

void filter_server_data(char** data, ssize_t* size)
{
}

void filter_deinit(void)
{
}
