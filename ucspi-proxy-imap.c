#include "ucspi-proxy.h"

const char program[] = "ucspi-proxy-imap-relay";
const char filter_usage[] = "command [args ...]";

extern void imap_filter_init(void);

void filter_init(int argc, char** argv)
{
  relay_init(argc, argv);
  imap_filter_init();
}

void filter_deinit(void)
{
}
