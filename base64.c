#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <msg/msg.h>
#include <str/str.h>

#define BASE64_INVALID ((unsigned)-1)

unsigned base64int(char ch)
{
  if (ch >= 'A' && ch <= 'Z')
    return ch - 'A';
  if (ch >= 'a' && ch <= 'z')
    return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9')
    return ch - '0' + 52;
  if (ch == '+')
    return 62;
  if (ch == '/')
    return 63;
  return BASE64_INVALID;
}

int base64decode(const char* data, unsigned long size, str* dest)
{
  unsigned char bin[3];
  unsigned char* ptr;
  unsigned d0;
  unsigned d1;
  unsigned d2;
  unsigned d3;
  
  dest->len = 0;
  while (size) {
    if (isspace(data[0])) size = 0;
    if (size < 4) break;
    if ((d0 = base64int(data[0])) == BASE64_INVALID) break;
    if ((d1 = base64int(data[1])) == BASE64_INVALID) break;
    ptr = bin;
    *ptr++ = (d0 << 2) | (d1 >> 4);
    if (data[2] != '=') {
      if ((d2 = base64int(data[2])) == BASE64_INVALID) break;
      *ptr++ = (d1 << 4) | (d2 >> 2);
      if (data[3] != '=') {
	if ((d3 = base64int(data[3])) == BASE64_INVALID) break;
	*ptr++ = (d2 << 6) | d3;
      }
    }
    data += 4;
    size -= 4;
    if (!str_catb(dest, (char*)bin, ptr - bin))
      die_oom(111);
  }
  return size ? 0 : 1;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[])
{
  int i;
  for (i = 1; i < argc; i++)
    printf("argv[%d] = '%s'\n", i, base64decode(argv[i], strlen(argv[i])));
  return 0;
}
#endif
