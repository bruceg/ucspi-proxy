#ifndef UCSPI_PROXY__H__
#define UCSPI_PROXY__H__

#define BUFSIZE 4096

typedef int bool;
#define true ((bool)(0==0))
#define false ((bool)0)

/* Functions and globals declared by the filter */
extern const char* filter_name;
extern void filter_init(int argc, char** argv);
extern void filter_client_data(char** data, ssize_t* size);
extern void filter_server_data(char** data, ssize_t* size);
extern void filter_deinit(void);

/* Helper functions */
extern void usage(const char*);

#endif
