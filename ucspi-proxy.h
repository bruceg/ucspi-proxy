#ifndef UCSPI_PROXY__H__
#define UCSPI_PROXY__H__

#define BUFSIZE 4096
#define CLIENT_IN 0
#define CLIENT_OUT 1
#define SERVER_IN 6
#define SERVER_OUT 7

typedef int bool;
#define true ((bool)(0==0))
#define false ((bool)0)

/* Functions and globals declared by the filter */
extern const char* filter_name;
extern const char* filter_usage;
extern void filter_init(int argc, char** argv);
extern void filter_deinit(void);

/* Functions from ucspi-proxy.c */
extern void usage(const char*);
extern bool opt_verbose;
extern void write_client(char*, ssize_t);
extern void write_server(char*, ssize_t);
extern bool add_filter(int fd, void(*filter)(char*, ssize_t),
		       void(*at_eof)(void));
extern bool del_filter(int fd);

#endif
