#ifndef UCSPI_PROXY__FILTER__H__
#define UCSPI_PROXY__FILTER__H__

struct filter 
{
  const char* name;
  int (*init)(int argc, char** argv);
  int (*client_data)(char** data, ssize_t* size);
  int (*server_data)(char** data, ssize_t* size);
  int (*deinit)(void);
};
typedef struct filter filter;

#endif
