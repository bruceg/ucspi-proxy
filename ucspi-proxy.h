#ifndef UCSPI_PROXY__H__
#define UCSPI_PROXY__H__

#include <sys/types.h>

#define BUFSIZE 4096
#define CLIENT_IN 0
#define CLIENT_OUT 1
extern int SERVER_IN;
extern int SERVER_OUT;

typedef int bool;
#define true ((bool)(0==0))
#define false ((bool)0)

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
extern void filter_init(int argc, char** argv);
extern void filter_deinit(void);

/* Functions from ucspi-proxy.c */
extern void usage(const char*);
extern bool opt_verbose;
extern pid_t pid;
extern void write_client(char*, ssize_t);
extern void write_server(char*, ssize_t);
extern bool add_filter(int fd, void(*filter)(char*, ssize_t),
		       void(*at_eof)(void));
extern bool del_filter(int fd);

#endif
