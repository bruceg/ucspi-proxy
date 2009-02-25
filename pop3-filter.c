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
static bool saw_auth_login = 0;
static bool saw_auth_plain = 0;
static str username;
static str linebuf;
static str tmpstr;

static void fixup_username(const char* msgprefix)
{
  if (local_name && str_findfirst(&username, AT) < 0) {
    str_catc(&username, AT);
    str_cats(&username, local_name);
  }
  msg2(msgprefix, username.s);
}

static const char* skipspace(const char* ptr)
{
  while (*ptr == ' ')
    ++ptr;
  return ptr;
}

static void handle_user(void)
{
  saw_command = 0;
  str_copyb(&username, linebuf.s + 5, linebuf.len - 6);
  str_rstrip(&username);
  fixup_username("USER ");
  str_copys(&linebuf, "USER ");
  str_cat(&linebuf, &username);
  str_catb(&linebuf, CRLF, 2);
}

static void handle_pass(void)
{
  saw_command = 1;
}

static void handle_auth_login_response(ssize_t offset)
{
  saw_auth_login = 0;
  if (!base64decode(linebuf.s + offset, linebuf.len + offset, &username))
    username.len = 0;
  else {
    fixup_username("AUTH LOGIN ");
    linebuf.len = offset;
    base64encode(username.s, username.len, &linebuf);
    str_catb(&linebuf, CRLF, 2);
  }
}

static void handle_auth_plain_response(ssize_t offset)
{
  int start;
  int end;

  saw_auth_plain = 0;
  if (base64decode(linebuf.s + offset, linebuf.len - offset, &tmpstr)) {
    /* tmpstr should now contain "AUTHORIZATION\0AUTHENTICATION\0PASSWORD" */
    if ((start = str_findfirst(&tmpstr, NUL)) >= 0
	&& (end = str_findnext(&tmpstr, NUL, ++start)) > start) {
      str_copyb(&username, tmpstr.s + start, end - start);
      fixup_username("AUTH PLAIN ");
      str_splice(&tmpstr, start, end - start, &username);
      linebuf.len = offset;
      base64encode(tmpstr.s, tmpstr.len, &linebuf);
      str_catb(&linebuf, CRLF, 2);
    }
  }
}

static void handle_auth(void)
{
  const char* ptr;

  ptr = skipspace(linebuf.s + 5);
  /* No parameter, so just pass it through. */
  if (*ptr == CR || *ptr == LF)
    return;
  saw_command = 1;
  if (str_case_starts(&linebuf, "AUTH LOGIN")) {
    if (linebuf.s[10] == ' ')
      handle_auth_login_response(11);
    else
      saw_auth_login = 1;
  }
  else if (str_case_starts(&linebuf, "AUTH PLAIN")) {
    if (linebuf.s[10] == ' ')
      handle_auth_plain_response(11);
    else
      saw_auth_plain = 1;
  }
}

static void filter_client_line(void)
{
  if (saw_auth_login)
    handle_auth_login_response(0);
  else if (saw_auth_plain)
    handle_auth_plain_response(0);
  else if (str_case_starts(&linebuf, "PASS "))
    handle_pass();
  else if (str_case_starts(&linebuf, "AUTH "))
    handle_auth();
  else if (str_case_starts(&linebuf, "USER "))
    handle_user();
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
