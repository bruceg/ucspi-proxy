#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static bool was_pasv = false;	/* Last command was "PASV" */
static bool pasv_mode = false;	/* Data transfer mode is passive */
static struct sockaddr_in locaddr; /* Local port address */
static struct sockaddr_in remaddr; /* Remote port address */
static int locsock = -1;	/* Local socket FD */
static int remsock = -1;	/* Remote socket FD */
static ssize_t copied = 0;	/* Number of bytes copied */

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
  int locsize = sizeof locaddr;
  locsock = socket(PF_INET, SOCK_STREAM, 0);
  if(locsock == -1)
    return false;
  remsock = socket(PF_INET, SOCK_STREAM, 0);
  if(remsock == -1)
    return false;
  
  memset(&locaddr, 0, locsize);
  locaddr.sin_family = AF_INET;
###NEED TO BIND TO AN ACTUAL ADDRESS, TO REPORT TO CLIENTS
  locaddr.sin_addr.s_addr = INADDR_ANY;
  locaddr.sin_port = 0;
  if(bind(locsock, (struct sockaddr *)&locaddr, locsize))
    return false;
  return getsockname(locsock, (struct sockaddr*)&locaddr, &locsize) != -1;
}

static void close_sockets(void)
{
  if(remsock != -1) {
    del_filter(remsock);
    close(remsock);
    remsock = -1;
  }
  if(locsock != -1) {
    del_filter(locsock);
    close(locsock);
    locsock = -1;
  }
}

static void write_remote(char* data, ssize_t size)
{
  write(remsock, data, size);
  copied += size;
}

static void write_local(char* data, ssize_t size)
{
  write(locsock, data, size);
  copied += size;
}

static void copy_eof(void)
{
  if(opt_verbose)
    fprintf(stderr, "%s: copied %d bytes\n", program, copied);
  close_sockets();
}

static bool start_copier(bool to_remote)
{
  if(listen(locsock, 1))
    return false;
  if(connect(remsock, &remaddr, sizeof remaddr)) {
    close_sockets();
    return false;
  }

  copied = 0;
  if(to_remote)
    add_filter(locsock, write_remote, copy_eof);
  else
    add_filter(remsock, write_local, copy_eof);
  return true;
}

static bool cmp_command(const char* data, const char* command)
{
  /* All commands are 4 characters long */
  return !strncasecmp(command, data, 4) &&
    (data[4] == ' ' || data[4] == '\r' || data[4] == '\n');
}

static void handle_port_command(char* data, ssize_t size)
{
  pasv_mode = false;
  
  /* Parse command: "PORT h1,h2,h3,h4,p1,p2" */
  if(!parse_remote_address(data, ' '))
    write_client(syntax_error, strlen(syntax_error));
  
  /* set up socket to listen on */
  else if(!make_local_socket())
    write_client(temp_error, strlen(temp_error));

  /* report socket address to server */
  else {
    format_local_address("PORT ", "\r\n");
    write_client(buf, strlen(buf));
  }
}

static void handle_pasv_response(char* data, ssize_t size)
{
  was_pasv = false;
  pasv_mode = true;
  
  /* Response: "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)." */
  if(atoi(data) != 227 || !parse_remote_address(data, '('))
    write_client(syntax_error, strlen(syntax_error));

  /* set up a socket */
  else if(!make_local_socket())
    write_client(temp_error, strlen(temp_error));
  
  /* report socket address to client */
  else {
    format_local_address("227 Entering Passive Mode(", ").\r\n");
    write_client(buf, strlen(buf));
  }
}

static void filter_client_data(char* data, ssize_t size)
{
  fprintf(stderr, "Client: %s", data);
  if(cmp_command(data, "PORT"))
    handle_port_command(data, size);
  else if(cmp_command(data, "PASV")) {
    was_pasv = true;
    write_server(data, size);
  }
  else if(cmp_command(data, "RETR") ||
	  cmp_command(data, "LIST") ||
	  cmp_command(data, "NLST")) {
    if(!start_copier(!pasv_mode)) {
      close_sockets();
      write_client(temp_error, strlen(temp_error));
    }
  }
  else if(cmp_command(data, "STOR") ||
	  cmp_command(data, "APPE") ||
	  cmp_command(data, "STOU")) {
    if(!start_copier(pasv_mode)) {
      close_sockets();
      write_client(temp_error, strlen(temp_error));
    }
  }
  else
    write_server(data, size);
}

static void filter_server_data(char* data, ssize_t size)
{
  fprintf(stderr, "Server: %s", data);
  if(was_pasv)
    handle_pasv_response(data, size);
  else
    write_client(data, size);
}

const char program[] = "ucspi-proxy-ftp";
const char filter_usage[] = "";

void filter_init(int argc, char** argv)
{
  if(argc > 0)
    usage("Too many arguments.");
  add_filter(CLIENT_IN, filter_client_data, 0);
  add_filter(SERVER_IN, filter_server_data, 0);
}

void filter_deinit(void)
{
}
