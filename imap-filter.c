#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <str/str.h>
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "* NO ";
const char filter_connfail_suffix[] = "\r\n";

extern char* base64decode(char* data, unsigned long size);

static bool saw_auth = 0;
static str label;
static str saved_label;
static str username;

static char* parse_label(char* data, ssize_t size)
{
  char* end;
  if ((end = memchr(data, ' ', size)) == 0) return 0;
  str_copyb(&label, data, end - data);
  return end+1;
}

static void filter_client_data(char* data, ssize_t size)
{
  char* cmd;
  char* ptr;

  cmd = parse_label(data, size);
  if(cmd) {
    /* If we see a "AUTH" or "LOGIN" command, save the preceding label
     * for reference when looking for the corresponding "OK" */
    if(!strncasecmp(cmd, "AUTHENTICATE ", 5)) {
      str_copy(&saved_label, &label);
      str_truncate(&label, 0);
      saw_auth = 1;
    }
    else if(!strncasecmp(cmd, "LOGIN ", 6)) {
      ptr = cmd + 6;
      while (isspace(*ptr))
	++ptr;
      str_copys(&username, ptr);
      ptr = username.s;
      while (!isspace(*ptr))
	++ptr;
      *ptr = 0;
      str_copy(&saved_label, &label);
      str_truncate(&label, 0);
    }
  }
  else if(saw_auth) {
    ptr = base64decode(data, size);
    if (ptr) {
      str_copys(&username, ptr);
      ptr = username.s;
      while (!isspace(*ptr)) ++ptr;
      *ptr = 0;
    }
    saw_auth = 0;
  }
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if(saved_label.len > 0) {
    /* Skip continuation data */
    if(data[0] != '+') {
      /* Check if the response is tagged with the saved label */
      const char* resp = parse_label(data, size);
      if(resp) {
	if(!str_diff(&label, &saved_label)) {
	  /* Check if the response was an OK */
	  if(!strncasecmp(resp, "OK ", 3))
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
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_FD, filter_server_data, 0);
}
