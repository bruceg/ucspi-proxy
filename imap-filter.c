#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bglibs/str.h>
#include "auth-lib.h"
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "* NO ";
const char filter_connfail_suffix[] = "\r\n";

extern const char* local_name;

static str label;
static str saved_label;

static int parse_label(const str* line)
{
  int end;
  if ((end = str_findfirst(line, ' ')) <= 0) return 0;
  str_copyb(&label, line->s, end);
  return end + 1;
}

static void handle_login(str* line, int offset)
{
  const char* start;
  const char* end;

  start = line->s + offset;
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
  make_username(start, end - start, "LOGIN ");
  str_splice(line, start-line->s, end-start, &username);
  str_copy(&saved_label, &label);
  str_truncate(&label, 0);
}

static void filter_client_line(str* line)
{
  int offset;
  const char* cmd;

  if (!handle_auth_response(line, 0)) {
    if ((offset = parse_label(line)) > 0) {
      cmd = line->s + offset;
      /* If we see a "AUTHENTICATE" or "LOGIN" command, save the preceding
       * label for reference when looking for the corresponding "OK" */
      if(!strncasecmp(cmd, "AUTHENTICATE ", 13)) {
	if (handle_auth_parameter(line, offset + 13))
	  str_copy(&saved_label, &label);
      }
      else if (!strncasecmp(cmd, "LOGIN ", 6))
	handle_login(line, offset + 6);
    }
  }
  write_line(line->s, line->len, write_server);
}

static void filter_server_line(str* line)
{
  if(saved_label.len > 0) {
    /* Skip continuation data */
    if (line->s[0] != '+') {
      int resp;
      /* Check if the response is tagged with the saved label */
      resp = parse_label(line);
      if(resp > 0) {
	if(!str_diff(&label, &saved_label)) {
	  log_line(line->s, line->len);
	  /* Check if the response was an OK */
	  if(!strncasecmp(line->s + resp, "OK ", 3))
	    accept_client(username.s);
	  else
	    deny_client(username.s);
	  str_truncate(&saved_label, 0);
	}
      }
    }
  }
  write_line(line->s, line->len, write_client);
}

void imap_filter_init(void)
{
  set_line_filter(CLIENT_IN, filter_client_line);
  set_line_filter(SERVER_FD, filter_server_line);
}
