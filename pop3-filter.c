#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <msg/msg.h>
#include <str/str.h>
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "-ERR ";
const char filter_connfail_suffix[] = "\r\n";

extern const char* local_name;

static bool saw_command = 0;
static bool saw_auth = 0;
static str username;
static str linebuf;

#define CR '\r'
#define LF '\n'

static void filter_client_data(char* data, ssize_t size)
{
  const char* ptr;
  const char* cr;
  if (!strncasecmp(data, "PASS ", 5))
    saw_command = 1;
  else if (!strncasecmp(data, "AUTH ", 5))
    saw_auth = saw_command = 1;
  else if (!strncasecmp(data, "USER ", 5)) {
    saw_command = 0;
    if ((cr = memchr(data, CR, size)) == 0)
      cr = memchr(data, LF, size);
    if (cr != 0) {
      ptr = data + 5;
      while (isspace(*ptr)) ++ptr;
      str_copyb(&username, ptr, cr-ptr);
      if (local_name && memchr(data, '@', size) == 0) {
	str_copyb(&linebuf, data, cr-data);
	str_catc(&linebuf, '@');
	str_catc(&username, '@');
	str_cats(&linebuf, local_name);
	str_cats(&username, local_name);
	str_catb(&linebuf, cr, size-(cr-data));
	data = linebuf.s;
	size = linebuf.len;
      }
      msg2("USER ", username.s);
    }
  }
  else if (saw_auth) {
    if (!base64decode(data, size, &username))
      username.len = 0;
    saw_auth = 0;
  }
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if (saw_command) {
    if (!strncasecmp(data, "+OK ", 4)) {
      accept_client(username.s);
      saw_command = 0;
    }
    else if (!strncasecmp(data, "-ERR ", 5)) {
      deny_client(username.s);
      saw_command = 0;
    }
  }
  write_client(data, size);
}

void pop3_filter_init(void)
{
  set_filter(CLIENT_IN, filter_client_data, 0);
  set_filter(SERVER_FD, filter_server_data, 0);
}
