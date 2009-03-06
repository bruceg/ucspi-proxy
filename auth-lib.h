#ifndef AUTH_LIB__H__
#define AUTH_LIB__H__

struct str;

extern struct str username;

extern void make_username(const char* start, ssize_t len,
			  const char* msgprefix);
extern int handle_auth_response(struct str* line, ssize_t offset);
extern int handle_auth_parameter(struct str* line, ssize_t offset);

#endif
