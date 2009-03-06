#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <msg/msg.h>
#include <str/str.h>
#include "auth-lib.h"
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "-ERR ";
const char filter_connfail_suffix[] = "\r\n";

extern const char* local_name;

static bool saw_command = 0;
static str linebuf;

static void handle_user(void)
{
  const char* ptr;
  const char* end;

  saw_command = 0;
  ptr = linebuf.s + 5;
  while (*ptr == ' ')
    ++ptr;
  end = linebuf.s + linebuf.len - 1;
  while (isspace(*end)) --end;
  make_username(ptr, end - ptr + 1, "USER ");
  str_copys(&linebuf, "USER ");
  str_cat(&linebuf, &username);
  str_catb(&linebuf, CRLF, 2);
}

static void handle_pass(void)
{
  saw_command = 1;
}

static void filter_client_line(void)
{
  if (!handle_auth_response(&linebuf, 0)) {
    if (str_case_starts(&linebuf, "PASS "))
      handle_pass();
    else if (str_case_starts(&linebuf, "AUTH "))
      saw_command = handle_auth_parameter(&linebuf, 5);
    else if (str_case_starts(&linebuf, "USER "))
      handle_user();
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
