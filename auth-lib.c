#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bglibs/msg.h>
#include <bglibs/str.h>
#include "auth-lib.h"
#include "ucspi-proxy.h"

extern const char* local_name;

str username = {0,0,0};

static bool saw_auth_login = 0;
static bool saw_auth_plain = 0;
static str tmpstr;
static str msgstr;

void make_username(const char* start, ssize_t len, const char* msgprefix)
{
  str_copyb(&username, start, len);
  if (local_name && str_findfirst(&username, AT) < 0) {
    str_catc(&username, AT);
    str_cats(&username, local_name);
  }
  str_copy2s(&msgstr, msgprefix, username.s);
  log_line(msgstr.s, msgstr.len);
}

static const char* skipspace(const char* ptr)
{
  while (*ptr == ' ')
    ++ptr;
  return ptr;
}

static void handle_auth_login_response(str* line, ssize_t offset)
{
  saw_auth_login = 0;
  if (!base64decode(line->s + offset, line->len - offset, &tmpstr))
    username.len = 0;
  else {
    make_username(tmpstr.s, tmpstr.len, "AUTH LOGIN ");
    line->len = offset;
    base64encode(username.s, username.len, line);
  }
}

static void handle_auth_plain_response(str* line, ssize_t offset)
{
  int start;
  int end;

  saw_auth_plain = 0;
  if (base64decode(line->s + offset, line->len - offset, &tmpstr)) {
    /* tmpstr should now contain "AUTHORIZATION\0AUTHENTICATION\0PASSWORD" */
    if ((start = str_findfirst(&tmpstr, NUL)) >= 0) {
      ++start;
      if ((end = str_findnext(&tmpstr, NUL, start)) > start) {
        make_username(tmpstr.s + start, end - start, "AUTH PLAIN ");
        str_splice(&tmpstr, start, end - start, &username);
        line->len = offset;
        base64encode(tmpstr.s, tmpstr.len, line);
      }
    }
  }
}

int handle_auth_response(str* line, ssize_t offset)
{
  if (saw_auth_login)
    handle_auth_login_response(line, offset);
  else if (saw_auth_plain)
    handle_auth_plain_response(line, offset);
  else
    return 0;
  return 1;
}

int handle_auth_parameter(str* line, ssize_t offset)
{
  const char* ptr;
  const char const* end = line->s + line->len;

  ptr = skipspace(line->s + offset);
  /* No parameter, so just pass it through. */
  if (ptr >= end)
    return 0;
  if (strncasecmp(ptr, "LOGIN", 5) == 0) {
    if (ptr[5] == ' ' && (ptr = skipspace(ptr + 5)) < end)
      handle_auth_login_response(line, ptr - line->s);
    else
      saw_auth_login = 1;
  }
  else if (strncasecmp(ptr, "PLAIN", 5) == 0) {
    if (ptr[5] == ' ' && (ptr = skipspace(ptr + 5)) < end)
      handle_auth_plain_response(line, ptr - line->s);
    else
      saw_auth_plain = 1;
  }
  return 1;
}
