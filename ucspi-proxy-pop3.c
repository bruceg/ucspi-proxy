#include <stdlib.h>
#include "ucspi-proxy.h"

const char program[] = "ucspi-proxy-pop3";
const char filter_usage[] = "[command [args ...]]";

const char* local_name = 0;

extern void pop3_filter_init(void);

void filter_init(int argc, char** argv)
{
  local_name = getenv("TCPLOCALHOST");
  relay_init(argc, argv);
  pop3_filter_init();
}

void filter_deinit(void)
{
}
