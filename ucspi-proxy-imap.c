#include <stdlib.h>
#include "ucspi-proxy.h"

const char program[] = "ucspi-proxy-imap";
const char filter_usage[] = "command [args ...]";

const char* local_name = 0;

extern void imap_filter_init(void);

void filter_init(int argc, char** argv)
{
  local_name = getenv("TCPLOCALHOST");
  relay_init(argc, argv);
  imap_filter_init();
}

void filter_deinit(void)
{
}
