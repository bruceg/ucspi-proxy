#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

extern void accept_client();

const char* client_ip = 0;
static char* label = 0;
static ssize_t label_size = 0;
static char* saved_label = 0;

static const char* parse_label(char* data, ssize_t size)
{
  const char* end = memchr(data, ' ', size);
  ssize_t len;
  if(!end)
    return 0;
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
  const char* cmd;
  if(!(cmd = parse_label(data, size)))
    return;
  /* If we see a "AUTH" or "LOGIN" command, save the preceding label
   * for reference when looking for the corresponding "OK" */
  if(!strncasecmp(cmd, "AUTH ", 5) ||
     !strncasecmp(cmd, "LOGIN ", 6)) {
    saved_label = label;
    label = 0;
  }
  write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  if(saved_label) {
    const char* resp;
    /* Skip continuation data */
    if(data[0] == '+')
      return;
    /* Check if the response is tagged with the saved label */
    resp = parse_label(data, size);
    if(!resp)
      return;
    if(!strcmp(label, saved_label)) {
      /* Check if the response was an OK */
      if(!strncasecmp(resp, "OK ", 3)) {
	accept_client();
      }
      free(saved_label);
      saved_label = 0;
    }
  }
  write_client(data, size);
}

const char* filter_name = "ucspi-proxy-imap-relay";
const char* filter_usage = "client-ip";

void filter_init(int argc, char** argv)
{
  if(argc != 1)
    usage("Incorrect usage.");
  client_ip = argv[0];
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_IN, filter_server_data, 0);
}

void filter_deinit(void)
{
}
