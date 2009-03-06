#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <msg/msg.h>
#include <str/str.h>
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "* NO ";
const char filter_connfail_suffix[] = "\r\n";

extern const char* local_name;

static bool saw_auth_login = 0;
static bool saw_auth_plain = 0;
static str label;
static str saved_label;
static str username;
static str linebuf;
static str tmpstr;

static int parse_label(void)
{
  int end;
  if ((end = str_findfirst(&linebuf, ' ')) <= 0) return 0;
  str_copyb(&label, linebuf.s, end);
  return end + 1;
}

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

static int iseol(char ch)
{
  return ch == CR || ch == LF;
}

static void handle_login(int offset)
{
  const char* start;
  const char* end;

  start = linebuf.s + offset;
  while (isspace(*start))
    ++start;
  if (*start == '"') {
    end = ++start;
    while (*end != '"')
      ++end;
  }
  else {
    end = start;
    while (!isspace(*end))
      ++end;
  }
  str_copyb(&username, start, end-start);
  fixup_username("LOGIN ");
  str_splice(&linebuf, start-linebuf.s, end-start, &username);
  str_copy(&saved_label, &label);
  str_truncate(&label, 0);
}

static void handle_auth_login_response(int offset)
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

static void handle_auth_plain_response(int offset)
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

static void handle_authenticate(int offset)
{
  const char* ptr;

  str_copy(&saved_label, &label);
  str_truncate(&label, 0);

  ptr = skipspace(linebuf.s + offset);
  /* No parameter, so just pass it through. */
  if (iseol(*ptr))
    return;
  if (strncasecmp(ptr, "LOGIN", 5) == 0) {
    if (ptr[5] == ' ' && !iseol(*(ptr = skipspace(ptr + 5))))
      handle_auth_login_response(ptr - linebuf.s);
    else
      saw_auth_login = 1;
  }
  else if (strncasecmp(ptr, "PLAIN", 5) == 0) {
    if (ptr[5] == ' ' && !iseol(*(ptr = skipspace(ptr + 5))))
      handle_auth_plain_response(ptr - linebuf.s);
    else
      saw_auth_plain = 1;
  }
}

static void filter_client_line(void)
{
  int offset;
  const char* cmd;

  if (saw_auth_login)
    handle_auth_login_response(0);
  else if (saw_auth_plain)
    handle_auth_plain_response(0);
  else if ((offset = parse_label()) > 0) {
    cmd = linebuf.s + offset;
    /* If we see a "AUTHENTICATE" or "LOGIN" command, save the preceding
     * label for reference when looking for the corresponding "OK" */
    if(!strncasecmp(cmd, "AUTHENTICATE ", 13))
      handle_authenticate(offset + 13);
    else if (!strncasecmp(cmd, "LOGIN ", 6))
      handle_login(offset + 6);
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
  if(saved_label.len > 0) {
    /* Skip continuation data */
    if(data[0] != '+') {
      /* Check if the response is tagged with the saved label */
      str_copyb(&linebuf, data, size);
      int resp = parse_label();
      if(resp > 0) {
	if(!str_diff(&label, &saved_label)) {
	  /* Check if the response was an OK */
	  if(!strncasecmp(linebuf.s + resp, "OK ", 3))
	    accept_client(username.s);
	  else
	    deny_client(username.s);
	  str_truncate(&saved_label, 0);
	}
      }
    }
  }
  write_client(data, size);
}

void imap_filter_init(void)
{
  set_filter(CLIENT_IN, filter_client_data, 0);
  set_filter(SERVER_FD, filter_server_data, 0);
}
