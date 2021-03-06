#include <bglibs/sysdeps.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <bglibs/fmt.h>
#include <bglibs/iobuf.h>
#include <bglibs/msg.h>
#include <bglibs/str.h>
#include "ucspi-proxy.h"

const int msg_show_pid = 1;

static unsigned long bytes_client_in = 0;
static unsigned long bytes_client_out = 0;
static unsigned long bytes_server_in = 0;
static unsigned long bytes_server_out = 0;
int opt_verbose = 0;
int opt_maxline = MAXLINE;
static unsigned opt_connect_timeout = 30;
static long opt_data_timeout = -1;
static const char* opt_source_addr = NULL;
pid_t pid;

int SERVER_FD = -1;

struct filter_node
{
  int fd;
  filter_fn filter_block;
  line_fn filter_line;
  write_fn writer;
  eof_fn at_eof;
  char* name;
  str linebuf;
  
  struct filter_node* next;
};

int nfilters = 0;
struct filter_node* filters = 0;

static bool new_filter(int fd, filter_fn block, line_fn line, write_fn writer, eof_fn at_eof)
{
  struct filter_node* newnode = malloc(sizeof *filters);
  if(!newnode)
    return false;
  memset(newnode, 0, sizeof *newnode);
  newnode->fd = fd;
  newnode->filter_block = block;
  newnode->filter_line = line;
  newnode->writer = writer;
  newnode->at_eof = at_eof;
  newnode->next = 0;
  if (fd == CLIENT_IN)
    newnode->name = "client";
  else if (fd == SERVER_FD)
    newnode->name = "server";
  else {
    newnode->name = malloc(4 + fmt_udec(0, fd));
    strcpy(newnode->name, "FD#");
    newnode->name[fmt_udec(newnode->name+3, fd)+3] = 0;
  }
  if(!filters)
    filters = newnode;
  else {
    struct filter_node* ptr = filters;
    while(ptr->next)
      ptr = ptr->next;
    ptr->next = newnode;
  }
  ++nfilters;
  return true;
}

bool set_filter(int fd, filter_fn filter, eof_fn at_eof)
{
  struct filter_node* node;
  for (node = filters; node != 0; node = node->next) {
    if (node->fd == fd) {
      /* Leave filter_line as-is, as a signal to handle_fd_line loop below */
      node->filter_block = filter;
      node->at_eof = at_eof;
      return true;
    }
  }
  return new_filter(fd, filter, NULL, NULL, at_eof);
}

bool set_line_filter(int fd, line_fn filter, write_fn writer)
{
  struct filter_node* node;
  for (node = filters; node != 0; node = node->next) {
    if (node->fd == fd) {
      node->filter_block = NULL;
      node->filter_line = filter;
      node->writer = writer;
      node->at_eof = NULL;
      return true;
    }
  }
  return new_filter(fd, NULL, filter, writer, NULL);
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
      str_free(&curr->linebuf);
      free(curr->name);
      free(curr);
      --nfilters;
      return true;
    }
  }
  return false;
}

static void handle_fd_line(struct filter_node* filter, char* buf, ssize_t rd)
{
  char* end;
  while ((end = memchr(buf, LF, rd)) != NULL) {
    ssize_t len = end++ - buf;
    if (len > 0 && buf[len - 1] == CR)
      --len;
    str_catb(&filter->linebuf, buf, len);
    filter->filter_line(&filter->linebuf);
    if (filter->writer != NULL && filter->linebuf.len > 0) {
      str_catb(&filter->linebuf, CRLF, 2);
      filter->writer(filter->linebuf.s, filter->linebuf.len);
    }
    filter->linebuf.len = 0;
    rd -= end - buf;
    buf = end;
    if (filter->filter_block != NULL) {
      /* Shunt off remaining data if the line function switched this filter to block mode. */
      filter->filter_line = NULL;
      filter->filter_block(buf, rd);
      return;
    }
  }
  str_catb(&filter->linebuf, buf, rd);
}

static void handle_fd(struct filter_node* filter)
{
  char buf[BUFSIZE+1];
  ssize_t rd = read(filter->fd, buf, BUFSIZE);
  if(rd == -1) {
    if (errno == EAGAIN || errno == EINTR)
      return;
    die2sys(1, "Error reading from ", filter->name);
    exit(1);
  }
  if(rd == 0) {
    if (opt_verbose) msg2(filter->name, " hangup");
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
    if (filter->filter_block)
      filter->filter_block(buf, rd);
    else
      handle_fd_line(filter, buf, rd);
  }
}

static void retry_write(const char* data, ssize_t size,
			int fd, const char* name, unsigned long* counter)
{
  ssize_t wr;
  iopoll_fd io;
  io.fd = fd;
  while(size > 0) {
    io.events = IOPOLL_WRITE;
    io.revents = 0;
    switch (iopoll_restart(&io, 1, -1)) {
    case -1:
      die1sys(1, "Poll failed");
    case 0:
      die2(1, "Connection closed during write to ", name);
    }
    switch (wr = write(fd, data, size)) {
    case 0:
      die2(1, "Short write to ", name);
    case -1:
      die2sys(1, "Error writing to ", name);
    default:
      data += wr;
      size -= wr;
      *counter += wr;
    }
  }
}

void write_client(const char* data, ssize_t size)
{
  retry_write(data, size, CLIENT_OUT, "client", &bytes_client_out);
}

void write_server(const char* data, ssize_t size)
{
  retry_write(data, size, SERVER_FD, "server", &bytes_server_out);
}

void write_line(const char* data, ssize_t size, write_fn fn)
{
  static str linebuf;
  str_copyb(&linebuf, data, size);
  str_catb(&linebuf, "\r\n", 2);
  fn(linebuf.s, linebuf.len);
}

void log_line(const char* data, ssize_t size)
{
  ssize_t i;
  int dots = 0;
  for (i = 0; i < size; i++) {
    if (data[i] == '\r' || data[i] == '\n')
      break;
    if (i >= opt_maxline) {
      dots = 1;
      break;
    }
  }
  char buf[i+1];
  memcpy(buf, data, i);
  if (dots)
    buf[i-1] = buf[i-2] = buf[i-3] = '.';
  buf[i] = 0;
  msg1(buf);
}

static void exitfn(void)
{
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
  filter_deinit();
}

void usage(const char* message)
{
  if(message)
    msg1(message);
  obuf_put5s(&errbuf, "usage: ", program, " [OPTIONS] [HOST PORT] ", filter_usage, "\n"
    "  -v           Output verbose messages\n"
    "  -s ADDR      Source address for outbound connections\n"
    "  -t NUM       Time out connecting after NUM seconds\n"
    "  -T NUM       Exit after NUM seconds of inactivity\n");
  obuf_flush(&errbuf);
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

void connect_server(const char* hostname, const char* port)
{
  if ((SERVER_FD = tcp_connect(hostname, port, opt_connect_timeout, opt_source_addr)) == -1)
    connfail();
}

static void parse_args(int argc, char* argv[])
{
  int opt;
  unsigned tmp;
  char* end;
  while((opt = getopt(argc, argv, "vl:s:t:T:")) != EOF) {
    switch(opt) {
    case 'v':
      opt_verbose++;
      break;
    case 'l':
      tmp = strtoul(optarg, &end, 10);
      if (tmp == 0 || *end != 0)
	usage("Invalid maximum line length");
      opt_maxline = tmp;
      break;
    case 's':
      opt_source_addr = optarg;
      break;
    case 't':
      tmp = strtoul(optarg, &end, 10);
      if (tmp == 0 || *end != 0)
	usage("Invalid connect timeout");
      opt_connect_timeout = tmp;
      break;
    case 'T':
      tmp = strtol(optarg, &end, 10);
      if (tmp <= 0 || *end != 0)
	usage("Invalid data timeout");
      opt_data_timeout = tmp;
      break;
    default:
      usage("Unknown option.");
      break;
    }
  }
  if (argc - optind == 0)
    SERVER_FD = 6;
  else if (argc - optind >= 2)
    connect_server(argv[optind], argv[optind+1]);
  else
    usage("Incorrect usage");

  optind += 2;
  filter_init(argc-optind, argv+optind);
}

int main(int argc, char* argv[])
{
  signal(SIGALRM, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  parse_args(argc, argv);
  atexit(exitfn);
  pid = getpid();
  for(;;) {
    struct filter_node* filter;
    iopoll_fd fds[nfilters];
    int i;
    for(i = 0, filter = filters; filter; ++i, filter = filter->next) {
      fds[i].fd = filter->fd;
      fds[i].events = IOPOLL_READ;
      fds[i].revents = 0;
    }
    while ((i = iopoll(fds, nfilters, opt_data_timeout * 1000)) < 0) {
      if(errno != EINTR)
        die1sys(111, "poll failed");
    }
    if (i == 0) {
      msg1("Timed out");
      break;
    }
    for(i = 0, filter = filters; filter; ++i, filter = filter->next)
      if(fds[i].revents) {
	handle_fd(filter);
	break;
      }
  }
}
