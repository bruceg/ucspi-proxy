#include <sys/types.h>
#include "filter.h"

static int ftp_init(int argc, char** argv)
{
}

static int ftp_client(char** data, ssize_t* size)
{
}

static int ftp_server(char** data, ssize_t* size)
{
}

static int ftp_deinit(void)
{
}

filter ftp_filter = {
  "ftp",
  ftp_init,
  ftp_client,
  ftp_server,
  ftp_deinit 
};
