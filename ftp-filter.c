#include <string.h>
#include <sys/types.h>
#include "ucspi-proxy.h"
#include <netinet/in.h>

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
}

static int was_pasv = 0;
static int was_port = 0;
static char* server_response = 0;
static char buf[4096];

static const char* syntax_error =
"500 Syntax error, command unrecognized.\r\n";

bool parse_address(const char* str, char prefix,
		   struct sockaddr_in* addr)
{
  unsigned bytes[6];
  int i;
  char* tmp;
  char* ptr = strchr(str, prefix);
  if(!ptr)
    return false;
  ++ptr;
  /* Expect 6 comma-seperated bytes here */
  for(i = 0; i < 6; i++) {
    bytes[i] = strtoul(ptr, &tmp, 10);
    if(i < 5 && *tmp != ',')
      return false;
  }
  if(!*ptr)
    return false;
  /* Turn the 6 bytes into an address and return */
  return false;
}

int make_socket(
void filter_client_data(char** data, ssize_t* size)
{
  if(!strncasecmp("PORT ", *data, 5)) {
    /* Command: "PORT h1,h2,h3,h4,p1,p2" */
    struct sockaddr_in addr;
    if(!parse_address(*data, ' ', &addr)) {
      server_response = syntax_error;
      *size = 0;
      return;
    }
    
    // set up socket
    // fork, child waits for a single connection
    // report socket address to server
    was_port = 1;
  }
  else if(!strncasecmp("PASV", *data, 4))
    was_pasv = 1;
  if(was_pasv || was_port) {
    write(2, "Client: ", 8);
    write(2, *data, *size);
  }
}

void filter_server_data(char** data, ssize_t* size)
{
  if(server_response) {
    *data = server_response;
    *size = strlen(server_response);
    server_response = 0;
  }
  else if(was_pasv) {
    /* Response: "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)." */
    struct sockaddr_in addr;
    if(!parse_address(*data, '(', &addr)) {
      *data = syntax_error;
      *size = strlen(syntax_error);
      return;
    }

    // set up a socket
    // fork, child waits for a single connection
    // report socket address to client
  }
  else if(was_port) {
    // report success of previous port operation
  }
  if(was_pasv || was_port) {
    write(2, "Server: ", 8);
    write(2, *data, *size);
    was_pasv = 0;
    was_port = 0;
  }
}

void filter_deinit(void)
{
  // kill any active children to close ports
}
