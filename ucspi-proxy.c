#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

static ssize_t bytes_client = 0;
static ssize_t bytes_server = 0;
static int opt_verbose = 0;
static const char* argv0;

#define DEBUG0(MSG) if(opt_verbose){fprintf(stderr,"%s: " MSG "\n",argv0);}
#define DEBUG1(MSG,ARG) if(opt_verbose){fprintf(stderr,"%s: " MSG "\n",argv0,ARG);}

ssize_t copy_fd(int in, int out, void (*filter)(char**,ssize_t*))
{
  char buf[BUFSIZE+1];
  char* ptr = buf;
  ssize_t rd = read(in, buf, BUFSIZE);
  if(rd == -1) {
    DEBUG1("Error encountered on FD %d", in);
    exit(1);
  }
  if(rd == 0) {
    DEBUG1("EOF on FD %d", in);
    exit(0);
  }
  buf[rd] = 0; /* Add an extra NUL for string searches in filter */
  filter(&ptr, &rd);
  if(write(out, ptr, rd) != rd) {
    DEBUG1("Short write on FD %d", out);
    exit(1);
  }
  return rd;
}

void exitfn(void)
{
  if(opt_verbose) {
    fprintf(stderr, "%s: client %d server %d\n",
	    argv0, bytes_client, bytes_server);
  }
  filter_deinit();
}

void usage(const char* message)
{
  if(message)
    fprintf(stderr, "%s: %s\n", argv0, message);
  fprintf(stderr, "usage: %s [-v]\n", argv0);
  exit(1);
}

void parse_args(int argc, char* argv[])
{
  int opt;
  argv0 = argv[0];
  while((opt = getopt(argc, argv, "v")) != EOF) {
    switch(opt) {
    case 'v':
      opt_verbose = 1;
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
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_SET(6, &fds);
    if(select(7, &fds, 0, 0, 0) == -1)
      usage("select failed!");
    if(FD_ISSET(0, &fds))
      bytes_client += copy_fd(0, 7, filter_client_data);
    if(FD_ISSET(6, &fds))
      bytes_server += copy_fd(6, 1, filter_server_data);
  }
}
