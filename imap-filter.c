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

static bool saw_auth = 0;
static str label;
static str saved_label;
static str username;
static str linebuf;

static int parse_label(void)
{
  int end;
  if ((end = str_findfirst(&linebuf, ' ')) <= 0) return 0;
  str_copyb(&label, linebuf.s, end);
  return end + 1;
}

static void fix_username(void)
{
  if (local_name && str_findfirst(&username, '@') < 0) {
    str_catc(&username, '@');
    str_cats(&username, local_name);
  }
}

static void filter_client_line(void)
{
  int offset;
  const char* cmd;
  const char* start;
  const char* end;
  char* ptr;

  if (saw_auth) {
    if (!base64decode(linebuf.s, linebuf.len, &username))
      username.len = 0;
    else {
      ptr = username.s;
      while (!isspace(*ptr))
	++ptr;
      *ptr = 0;
    }
    saw_auth = 0;
  }
  else if ((offset = parse_label()) > 0) {
    cmd = linebuf.s + offset;
    /* If we see a "AUTHENTICATE" or "LOGIN" command, save the preceding
     * label for reference when looking for the corresponding "OK" */
    if(!strncasecmp(cmd, "AUTHENTICATE ", 5)) {
      str_copy(&saved_label, &label);
      str_truncate(&label, 0);
      saw_auth = 1;
    }
    else if (!strncasecmp(cmd, "LOGIN ", 6)) {
      start = cmd + 6;
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
      fix_username();
      str_splice(&linebuf, start-linebuf.s, end-start, &username);
      str_copy(&saved_label, &label);
      str_truncate(&label, 0);
      msg2("LOGIN ", username.s);
    }
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
