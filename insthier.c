#include <installer.h>
#include "conf_bin.c"
#include "conf_man.c"

void insthier(void)
{
  int bin, man, man1;
  bin = opendir(conf_bin);
  c(bin, "ucspi-proxy-http-xlate",    -1, -1, 0755);
  c(bin, "ucspi-proxy-imap",          -1, -1, 0755);
  c(bin, "ucspi-proxy-log",           -1, -1, 0755);
  c(bin, "ucspi-proxy-pop3",          -1, -1, 0755);
  c(bin, "ucspi-proxy",               -1, -1, 0755);
  man = opendir(conf_man);
  man1 = d(man, "man1", -1, -1, 0755);
  c(man1, "ucspi-proxy-http-xlate.1", -1, -1, 0644);
  c(man1, "ucspi-proxy.1",            -1, -1, 0644);
}
