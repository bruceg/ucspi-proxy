#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <fmt/number.h>
#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/str.h>
#include "ucspi-proxy.h"

const int msg_show_pid = 1;

static unsigned long bytes_client_in = 0;
static unsigned long bytes_client_out = 0;
static unsigned long bytes_server_in = 0;
static unsigned long bytes_server_out = 0;
int opt_verbose = 0;
static unsigned opt_timeout = 30;
pid_t pid;

int SERVER_FD = -1;

struct filter_node
{
  int fd;
  filter_fn filter;
  eof_fn at_eof;
  
  struct filter_node* next;
};

struct filter_node* filters = 0;

static bool new_filter(int fd, filter_fn filter, eof_fn at_eof)
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

bool set_filter(int fd, filter_fn filter, eof_fn at_eof)
{
  struct filter_node* node;
  for (node = filters; node != 0; node = node->next) {
    if (node->fd == fd) {
      node->filter = filter;
      node->at_eof = at_eof;
      return true;
    }
  }
  return new_filter(fd, filter, at_eof);
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

static void msg2n(const char* a, unsigned long b)
{
  char num[FMT_ULONG_LEN];
  num[fmt_udec(num, b)] = 0;
  msg2(a, num);
}

static void handle_fd(struct filter_node* filter)
{
  char buf[BUFSIZE+1];
  ssize_t rd = read(filter->fd, buf, BUFSIZE);
  if(rd == -1) {
    if (errno == EAGAIN || errno == EINTR)
      return;
    if (opt_verbose) msg2n("Error encountered on FD ", filter->fd);
    exit(1);
  }
  if(rd == 0) {
    if (opt_verbose) msg2n("EOF on FD ", filter->fd);
    if(filter->at_eof)
      filter->at_eof();
    else
      exit(0);
  }
  else {
    buf[rd] = 0; /* Add an extra NUL for string searches in filter */
    if (filter->fd == CLIENT_IN)
      bytes_client_in += rd;
    else if (filter->fd == SERVER_FD)
      bytes_server_in += rd;
    filter->filter(buf, rd);
  }
}

void write_client(const char* data, ssize_t size)
{
  while(size > 0) {
    ssize_t wr = write(CLIENT_OUT, data, size);
    switch(wr) {
    case 0:
      if (opt_verbose) msg1("Short write to client");
      exit(1);
    case -1:
      if (opt_verbose) msg1("Write to client failed");
      exit(1);
    default:
      data += wr;
      size -= wr;
      bytes_client_out += wr;
    }
  }
}

void writes_client(const char* data)
{
  write_client(data, strlen(data));
}

void write_server(const char* data, ssize_t size)
{
  if(write(SERVER_FD, data, size) != size) {
    if (opt_verbose) msg1("Short write to server");
    exit(1);
  }
  bytes_server_out += size;
}

void writes_server(const char* data)
{
  write_server(data, strlen(data));
}

static void exitfn(void)
{
  if (opt_verbose) {
    char line[42+FMT_ULONG_LEN*4];
    int i;
    memcpy(line, "bytes: client->server ", 22); i = 22;
    i += fmt_udec(line+i, bytes_client_in);
    line[i++] = '-'; line[i++] = '>';
    i += fmt_udec(line+i, bytes_server_out);
    memcpy(line+i, " server->client ", 16); i += 16;
    i += fmt_udec(line+i, bytes_server_in);
    line[i++] = '-'; line[i++] = '>';
    i += fmt_udec(line+i, bytes_client_out);
    line[i] = 0;
    msg1(line);
  }
  filter_deinit();
}

void usage(const char* message)
{
  if(message)
    msg1(message);
  obuf_put4s(&errbuf, "usage: ", program,
	     " [-v] [-t timeout] host port ", filter_usage);
  obuf_endl(&errbuf);
  exit(1);
}

static void connfail(void)
{
  str buf = {0,0,0};
  str_copy4s(&buf,
	     filter_connfail_prefix,
	     "Connection to server failed: ",
	     strerror(errno),
	     filter_connfail_suffix);
  write_client(buf.s, buf.len);
  exit(0);
}

static void parse_args(int argc, char* argv[])
{
  int opt;
  unsigned tmp;
  char* end;
  while((opt = getopt(argc, argv, "vt:")) != EOF) {
    switch(opt) {
    case 'v':
      opt_verbose++;
      break;
    case 't':
      tmp = strtoul(optarg, &end, 10);
      if (tmp == 0 || *end != 0)
	usage("Invalid timeout");
      opt_timeout = tmp;
      break;
    default:
      usage("Unknown option.");
      break;
    }
  }
  if (argc - optind < 2)
    usage("Missing host and port");
  if ((SERVER_FD = tcp_connect(argv[optind], argv[optind+1],
			       opt_timeout)) == -1)
    connfail();
  optind += 2;
  filter_init(argc-optind, argv+optind);
}

int main(int argc, char* argv[])
{
  fd_set fds;
  signal(SIGALRM, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  parse_args(argc, argv);
  atexit(exitfn);
  pid = getpid();
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
    while(select(maxfd+1, &fds, 0, 0, 0) == -1) {
      if(errno != EINTR)
	usage("select failed!");
    }
    for(filter = filters; filter; filter = filter->next)
      if(FD_ISSET(filter->fd, &fds)) {
	handle_fd(filter);
	break;
      }
  }
}
