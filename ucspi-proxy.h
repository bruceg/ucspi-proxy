#ifndef UCSPI_PROXY__H__
#define UCSPI_PROXY__H__

#include <sys/types.h>

#define BUFSIZE 4096
#define CLIENT_IN 0
#define CLIENT_OUT 1
extern int SERVER_FD;

typedef int bool;
#define true ((bool)(0==0))
#define false ((bool)0)

typedef void (*filter_fn)(char*, ssize_t);
typedef void (*eof_fn)(void);

#ifdef DEBUG
#define DEBUGG(ARGS) {fprintf ARGS;}
#else
#define DEBUGG(ARGS) do{}while(0)
#endif
#define DEBUG0(S) DEBUGG((stderr, "%s: " S "\n",program))
#define DEBUG1(S,A) DEBUGG((stderr, "%s: " S "\n",program,A))
#define DEBUG2(S,A,B) DEBUGG((stderr, "%s: " S "\n",program,A,B))
#define DEBUG3(S,A,B,C) DEBUGG((stderr, "%s: " S "\n",program,A,B,C))
#define DEBUG4(S,A,B,C,D) DEBUGG((stderr, "%s: " S "\n",program,A,B,C,D))

#define MSGG(ARGS) if(opt_verbose){fprintf ARGS;}
#define MSG0(S) MSGG((stderr, "[%d] %s: " S "\n",pid,program))
#define MSG1(S,A) MSGG((stderr, "[%d] %s: " S "\n",pid,program,A))
#define MSG2(S,A,B) MSGG((stderr, "[%d] %s: " S "\n",pid,program,A,B))
#define MSG3(S,A,B,C) MSGG((stderr, "[%d] %s: " S "\n",pid,program,A,B,C))
#define MSG4(S,A,B,C,D) MSGG((stderr, "[%d] %s: " S "\n",pid,program,A,B,C,D))

/* Functions and globals declared by the filter */
extern const char program[];
extern const char filter_usage[];
extern const char filter_connfail_prefix[];
extern const char filter_connfail_suffix[];
extern void filter_init(int argc, char** argv);
extern void filter_deinit(void);

/* Functions from ucspi-proxy.c */
extern void usage(const char*);
extern bool opt_verbose;
extern pid_t pid;
extern void write_client(const char*, ssize_t);
extern void write_server(const char*, ssize_t);
extern void writes_client(const char*);
extern void writes_server(const char*);
extern bool add_filter(int fd, filter_fn filter, eof_fn at_eof);
extern bool del_filter(int fd);

/* Functions from tcp-connect.c */
extern int tcp_connect(void);

/* Functions from relay-filter.c */
extern void relay_init(int argc, char* argv[]);
extern void accept_client(const char* username);
extern void deny_client(const char* username);

#endif
