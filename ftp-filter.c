#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
}

static bool was_pasv = false;	/* Last command was "PASV" */
static bool pasv_mode = false;	/* Data transfer mode is passive */
static struct sockaddr_in locaddr; /* Local port address */
static struct sockaddr_in remaddr; /* Remote port address */
static int sockfd = -1;		/* Socket FD */
static pid_t child = 0;		/* Forked child PID */

static const char* server_response = 0;	/* Generated server response string */
static char buf[4096];		/* Generated server response buffer */

static const char* syntax_error =
"500 Syntax error, command unrecognized.\r\n";
static const char* temp_error =
"425 Can't open data connection.\r\n";

static bool parse_remote_address(const char* str, char prefix)
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
  memset(&remaddr, 0, sizeof remaddr);
  remaddr.sin_family = AF_INET;
  remaddr.sin_addr.s_addr = htonl((bytes[0] << 24) |
				  (bytes[1] << 16) |
				  (bytes[2] << 8) |
				  bytes[3]);
  remaddr.sin_port = htons((bytes[4] << 8) | bytes[5]);
  return true;
}

static void format_local_address(const char* prefix, const char* suffix)
{
  unsigned ip = ntohl(locaddr.sin_addr.s_addr);
  unsigned port = ntohs(locaddr.sin_port);
  sprintf(buf, "%s%u,%u,%u,%u,%u,%u%s", prefix,
	  (ip>>24)&0xff, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff,
	  (port>>8)&0xff, port&0xff, suffix);
}

static bool make_local_socket(void)
{
  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if(sockfd == -1)
    return false;
  memset(&locaddr, 0, sizeof locaddr);
  locaddr.sin_family = AF_INET;
  locaddr.sin_addr.s_addr = INADDR_ANY;
  locaddr.sin_port = 0;
  return bind(sockfd, (struct sockaddr *)&locaddr, sizeof locaddr) != -1;
}

static void run_copier(bool to_remote)
{
  exit(111);
}

static bool fork_copier(bool to_remote)
{
  child = fork();
  switch(child) {
  case -1:
    return false;
  case 0:
    run_copier(to_remote);
    return true;
  default:
    return true;
  }
}

static bool cmp_command(const char* data, const char* command)
{
  /* All commands are 4 characters long */
  return !strncasecmp(command, data, 4) &&
    (data[4] == ' ' || data[4] == '\r' || data[4] == '\n');
}

static void handle_port_command(char** data, ssize_t* size)
{
  pasv_mode = false;
  
  /* Parse command: "PORT h1,h2,h3,h4,p1,p2" */
  if(!parse_remote_address(*data, ' ')) {
    server_response = syntax_error;
    *size = 0;
    return;
  }
  
  /* set up socket to listen on */
  if(!make_local_socket()) {
    server_response = temp_error;
    *size = 0;
    return;
  }

  /* report socket address to server */
  format_local_address("PORT ", "\r\n");
  *data = buf;
  *size = strlen(buf);
}

static void handle_pasv_response(char** data, ssize_t* size)
{
  /* Response: "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)." */
  if(atoi(*data) != 227 ||
     !parse_remote_address(*data, '(')) {
    *data = (char*)syntax_error;
    *size = strlen(syntax_error);
    return;
  }

  /* set up a socket */
  if(!make_local_socket()) {
    *data = (char*) temp_error;
    *size = strlen(temp_error);
    return;
  }
  
  /* report socket address to client */
  format_local_address("227 Entering Passive Mode(", ").\r\n");
  *data = buf;
  *size = strlen(buf);
}

void filter_client_data(char** data, ssize_t* size)
{
  if(cmp_command(*data, "PORT"))
    handle_port_command(data, size);
  else if(cmp_command(*data, "PASV"))
    was_pasv = true;
  else if(cmp_command(*data, "RETR") ||
	  cmp_command(*data, "LIST") ||
	  cmp_command(*data, "NLST")) {
    if(!fork_copier(!pasv_mode)) {
      close(sockfd);
      sockfd = -1;
      server_response = temp_error;
      *size = 0;
    }
  }
  else if(cmp_command(*data, "STOR") ||
	  cmp_command(*data, "APPE") ||
	  cmp_command(*data, "STOU")) {
    if(!fork_copier(pasv_mode)) {
      close(sockfd);
      sockfd = -1;
      server_response = temp_error;
      *size = 0;
    }
  }
}

void filter_server_data(char** data, ssize_t* size)
{
  if(server_response) {
    *data = (char*)server_response;
    *size = strlen(server_response);
    server_response = 0;
  }
  else if(pasv_mode)
    handle_pasv_response(data, size);
}

void filter_deinit(void)
{
  if(child)
    kill(child, SIGTERM);
}
