#ifndef UCSPI_PROXY__H__
#define UCSPI_PROXY__H__

#define BUFSIZE 4096

extern void filter_init(int argc, char** argv);
extern void filter_client_data(char** data, ssize_t* size);
extern void filter_server_data(char** data, ssize_t* size);
extern void filter_deinit(void);
extern void usage(const char*);

#endif
