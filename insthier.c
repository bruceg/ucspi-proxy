#include <installer.h>
#include "conf_bin.c"

void insthier(void)
{
	const int bin = opendir(conf_bin);
	c(bin, "ucspi-proxy-http-xlate", -1, -1, 0755);
	c(bin, "ucspi-proxy-imap-relay", -1, -1, 0755);
	c(bin, "ucspi-proxy-log",        -1, -1, 0755);
	c(bin, "ucspi-proxy-pop3-relay", -1, -1, 0755);
	c(bin, "ucspi-proxy",            -1, -1, 0755);
}
