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
#define CRLF "\r\n"
#define AT '@'

static void filter_client_line(void)
{
  const char* ptr;
  if (saw_auth) {
    if (!base64decode(linebuf.s, linebuf.len, &username))
      username.len = 0;
    saw_auth = 0;
  }
  else if (str_case_starts(&linebuf, "PASS "))
    saw_command = 1;
  else if (str_case_starts(&linebuf, "AUTH "))
    saw_auth = saw_command = 1;
  else if (str_case_starts(&linebuf, "USER ")) {
    saw_command = 0;
    ptr = linebuf.s + 5;
    while (isspace(*ptr)) ++ptr;
    str_copyb(&username, ptr, linebuf.s + linebuf.len - ptr);
    str_rstrip(&username);
    if (local_name && str_findfirst(&username, AT) < 0) {
      str_catc(&username, AT);
      str_cats(&username, local_name);
    }
    msg2("USER ", username.s);
    str_copys(&linebuf, "USER ");
    str_cat(&linebuf, &username);
    str_catb(&linebuf, CRLF, 2);
  }
}

static void filter_client_data(char* data, ssize_t size)
{
  char* lf;
  while ((lf = memchr(data, LF, size)) != 0) {
    str_catb(&linebuf, data, lf - data + 1);
    filter_client_line();
    write_server(linebuf.s, linebuf.len);
    linebuf.len = 0;
    size -= lf - data + 1;
    data = lf + 1;
  }
  str_catb(&linebuf, data, size);
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
