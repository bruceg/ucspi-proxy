#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static ssize_t bytes_client = 0;
static ssize_t bytes_server = 0;
bool opt_verbose = false;

#define DEBUG0(MSG) if(opt_verbose){fprintf(stderr,"%s: " MSG "\n",filter_name);}
#define DEBUG1(MSG,ARG) if(opt_verbose){fprintf(stderr,"%s: " MSG "\n",filter_name,ARG);}

struct filter_node
{
  int fd;
  void (*filter)(char*, ssize_t);
  void (*at_eof)(void);
  
  struct filter_node* next;
};

struct filter_node* filters = 0;

bool add_filter(int fd, void(*filter)(char*, ssize_t), void(*at_eof)(void))
{
  struct filter_node* newnode = malloc(sizeof *filters);
  if(!newnode)
    return false;
  newnode->fd = fd;
  newnode->filter = filter;
  newnode->at_eof = at_eof;
  newnode->next = 0;
  if(!filters)
    filters = newnode;
  else {
    struct filter_node* ptr = filters;
    while(ptr->next)
      ptr = ptr->next;
    ptr->next = newnode;
  }
  return true;
}

bool del_filter(int fd)
{
  struct filter_node* prev = 0;
  struct filter_node* curr = filters;
  while(curr) {
    if(curr->fd == fd) {
      if(prev)
	prev->next = curr->next;
      else
	filters = curr->next;
      free(curr);
      return true;
    }
  }
  return false;
}

static void handle_fd(struct filter_node* filter)
{
  char buf[BUFSIZE+1];
  ssize_t rd = read(filter->fd, buf, BUFSIZE);
  if(rd == -1) {
    DEBUG1("Error encountered on FD %d", filter->fd);
    exit(1);
  }
  if(rd == 0) {
    if(filter->at_eof)
      filter->at_eof();
    else {
      DEBUG1("EOF on FD %d", filter->fd);
      exit(0);
    }
  }
  else {
    buf[rd] = 0; /* Add an extra NUL for string searches in filter */
    filter->filter(buf, rd);
  }
}

void write_client(char* data, ssize_t size)
{
  if(write(CLIENT_OUT, data, size) != size) {
    DEBUG0("Short write to client");
    exit(1);
  }
  bytes_client += size;
}

void write_server(char* data, ssize_t size)
{
  if(write(SERVER_OUT, data, size) != size) {
    DEBUG0("Short write to server");
    exit(1);
  }
  bytes_server += size;
}

static void exitfn(void)
{
  if(opt_verbose) {
    fprintf(stderr, "%s: client %d server %d\n",
	    filter_name, bytes_client, bytes_server);
  }
  filter_deinit();
}

void usage(const char* message)
{
  if(message)
    fprintf(stderr, "%s: %s\n", filter_name, message);
  fprintf(stderr, "usage: %s [-v] %s", filter_name, filter_usage);
  exit(1);
}

static void parse_args(int argc, char* argv[])
{
  int opt;
  while((opt = getopt(argc, argv, "v")) != EOF) {
    switch(opt) {
    case 'v':
      opt_verbose = true;
      break;
    default:
      usage("Unknown option.");
      break;
    }
  }
  filter_init(argc-optind, argv+optind);
}

int main(int argc, char* argv[])
{
  fd_set fds;
  parse_args(argc, argv);
  atexit(exitfn);
  for(;;) {
    struct filter_node* filter;
    int maxfd = -1;
    FD_ZERO(&fds);
    for(filter = filters; filter; filter = filter->next) {
      int fd = filter->fd;
      FD_SET(fd, &fds);
      if(fd > maxfd)
	maxfd = fd;
    }
    if(select(maxfd+1, &fds, 0, 0, 0) == -1)
      usage("select failed!");
    for(filter = filters; filter; filter = filter->next)
      if(FD_ISSET(filter->fd, &fds))
	handle_fd(filter);
  }
}
