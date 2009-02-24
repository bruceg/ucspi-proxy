#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>

#include <base64/base64.h>
#include <msg/msg.h>
#include <str/str.h>

#include "ucspi-proxy.h"

#define BASE64_INVALID ((unsigned)-1)

int base64decode(const char* data, unsigned long size, str* dest)
{
  unsigned char bin[3];
  int decoded;
  
  dest->len = 0;
  while (size) {
    if (isspace(data[0])) size = 0;
    if (size < 4) break;
    if ((decoded = base64_decode_part(data, bin)) <= 0) break;
    data += 4;
    size -= 4;
    if (!str_catb(dest, (char*)bin, decoded))
      die_oom(111);
  }
  return size ? 0 : 1;
}

int base64encode(const char* data, unsigned long size, str* dest)
{
  return base64_encode_line((const unsigned char*)data, size, dest);
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[])
{
  int i;
  int r;
  str d = {0,0,0};
  for (i = 1; i < argc; i++) {
    int r = base64decode(argv[i], strlen(argv[i]), &d);
    printf("argv[%d] = %d: '%s'\n", i, r, d.s);
  }
  return 0;
}
#endif
