#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bglibs/str.h>
#include "auth-lib.h"
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "-ERR ";
const char filter_connfail_suffix[] = "\r\n";

extern const char* local_name;

static bool saw_command = 0;

static void handle_user(str* line)
{
  const char* ptr;
  const char* end;

  saw_command = 0;
  ptr = line->s + 5;
  while (*ptr == ' ')
    ++ptr;
  end = line->s + line->len - 1;
  while (isspace(*end)) --end;
  make_username(ptr, end - ptr + 1, "USER ");
  str_copys(line, "USER ");
  str_cat(line, &username);
}

static void handle_pass(void)
{
  saw_command = 1;
}

static void filter_client_line(str* line)
{
  if (!handle_auth_response(line, 0)) {
    if (str_case_starts(line, "PASS "))
      handle_pass();
    else if (str_case_starts(line, "AUTH "))
      saw_command = handle_auth_parameter(line, 5);
    else if (str_case_starts(line, "USER "))
      handle_user(line);
  }
  write_line(line->s, line->len, write_server);
}

static void filter_server_line(str* line)
{
  if (saw_command) {
    log_line(line->s, line->len);
    if (!strncasecmp(line->s, "+OK ", 4)) {
      accept_client(username.s);
      saw_command = 0;
    }
    else if (!strncasecmp(line->s, "-ERR ", 5)) {
      deny_client(username.s);
      saw_command = 0;
    }
  }
  write_line(line->s, line->len, write_client);
}

void pop3_filter_init(void)
{
  set_line_filter(CLIENT_IN, filter_client_line);
  set_line_filter(SERVER_FD, filter_server_line);
}
