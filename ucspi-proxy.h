#ifndef UCSPI_PROXY__H__
#define UCSPI_PROXY__H__

#include <sys/types.h>

#define BUFSIZE 4096
#define CLIENT_IN 0
#define CLIENT_OUT 1
extern int SERVER_FD;

struct str;

typedef int bool;
#define true ((bool)(0==0))
#define false ((bool)0)

typedef void (*filter_fn)(char*, ssize_t);
typedef void (*eof_fn)(void);

/* Functions and globals declared by the filter */
extern const char program[];
extern const char filter_usage[];
extern const char filter_connfail_prefix[];
extern const char filter_connfail_suffix[];
extern void filter_init(int argc, char** argv);
extern void filter_deinit(void);

/* Functions from base64.c */
extern int base64decode(const char* data, unsigned long size, struct str* dest);
extern int base64encode(const char* data, unsigned long size, struct str* dest);

/* Functions from ucspi-proxy.c */
extern void usage(const char*);
extern int opt_verbose;
extern pid_t pid;
extern void write_client(const char*, ssize_t);
extern void write_server(const char*, ssize_t);
extern void writes_client(const char*);
extern void writes_server(const char*);
extern bool set_filter(int fd, filter_fn filter, eof_fn at_eof);
extern bool del_filter(int fd);

/* Functions from tcp-connect.c */
extern int tcp_connect(const char*, const char*, unsigned);

/* Functions from relay-filter.c */
extern void relay_init(int argc, char* argv[]);
extern void accept_client(const char* username);
extern void deny_client(const char* username);

#endif
