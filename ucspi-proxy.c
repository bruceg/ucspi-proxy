#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static ssize_t bytes_client = 0;
static ssize_t bytes_server = 0;
static int opt_verbose = 0;

ssize_t copy_fd(int in, int out)
{
  char buf[4096];
  ssize_t rd = read(in, buf, 4096);
  if(rd == -1 || rd == 0)
    exit(0);
  if(write(out, buf, rd) != rd)
    exit(1);
  return rd;
}

void exitfn(void)
{
  if(opt_verbose) {
    fprintf(stderr, "ucspi-proxy: client %d server %d\n",
	    bytes_client, bytes_server);
  }
}

int main(void)
{
  fd_set fds;
  atexit(exitfn);
  for(;;) {
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    FD_SET(6, &fds);
    if(select(7, &fds, 0, 0, 0) == -1)
      return 1;
    if(FD_ISSET(0, &fds))
      bytes_client += copy_fd(0, 7);
    if(FD_ISSET(6, &fds))
      bytes_server += copy_fd(6, 1);
  }
}
