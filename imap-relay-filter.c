#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

const char filter_connfail_prefix[] = "* NO ";
const char filter_connfail_suffix[] = "\r\n";

extern void accept_client(const char* username);
extern void deny_client(void);
extern void relay_init(int argc, char** argv);
extern char* base64decode(char* data, unsigned long size);

static bool saw_auth = 0;
static char* label = 0;
static ssize_t label_size = 0;
static char* saved_label = 0;
static char* username = 0;

static char* parse_label(char* data, ssize_t size)
{
  char* end;
  ssize_t len;
  if ((end = memchr(data, ' ', size)) == 0) return 0;
  len = end - data;
  if(!label || len > label_size) {
    free(label);
    label = malloc(len+1);
  }
  memcpy(label, data, len);
  label[len] = 0;
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
      saved_label = label;
      label = 0;
      saw_auth = 1;
    }
    else if(!strncasecmp(cmd, "LOGIN ", 6)) {
      ptr = cmd + 6;
      while (isspace(*ptr))
	++ptr;
      ptr = username = strdup(ptr);
      while (!isspace(*ptr))
	++ptr;
      *ptr = 0;
      saved_label = label;
      label = 0;
    }
  }
  else if(saw_auth) {
    ptr = base64decode(data, size);
    if (ptr) {
      username = ptr;
      while (!isspace(*ptr)) ++ptr;
      *ptr = 0;
    }
    saw_auth = 0;
  }
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if(saved_label) {
    /* Skip continuation data */
    if(data[0] != '+') {
      /* Check if the response is tagged with the saved label */
      const char* resp = parse_label(data, size);
      if(resp) {
	if(!strcmp(label, saved_label)) {
	  /* Check if the response was an OK */
	  if(!strncasecmp(resp, "OK ", 3))
	    accept_client(username);
	  else
	    deny_client();
	  free(saved_label);
	  saved_label = 0;
	}
      }
    }
  }
  write_client(data, size);
}

void filter_init(int argc, char** argv)
{
  relay_init(argc, argv);
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_FD, filter_server_data, 0);
}

void filter_deinit(void)
{
}
