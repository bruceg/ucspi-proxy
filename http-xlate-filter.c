#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "ucspi-proxy.h"

// FIXME: make sure this is the right error number
const char filter_connfail_prefix[] = "503 ";
const char filter_connfail_suffix[] = "\r\n";

#define CR ((char)13)
#define LF ((char)10)

struct replacement
{
  const char* src;
  size_t srclen;
  const char* dst;
  size_t dstlen;
};
typedef struct replacement replacement;

struct buffer 
{
  size_t len;
  char* buf;
  struct buffer* next;
};
typedef struct buffer buffer;

struct buffer_list
{
  buffer* head;
  buffer* tail;
};
typedef struct buffer_list buffer_list;

static int content_html;
static size_t content_length;
static size_t content_length_offset;
static size_t content_length_end;
static size_t content_left;
static buffer_list buffers;
static int state = 0;

static replacement* repls;

static buffer* header = 0;

static size_t min_src_repl;
static size_t max_dst_repl;

static void calc_lengths(void) 
{
  replacement* ptr;
  for (ptr = repls; ptr->src; ++ptr) {
    ptr->srclen = strlen(ptr->src);
    ptr->dstlen = strlen(ptr->dst);
  }
  min_src_repl = strlen(repls->src);
  max_dst_repl = strlen(repls->dst);
  for (ptr = repls+1; ptr->src; ++ptr) {
    if (ptr->srclen < min_src_repl) min_src_repl = ptr->srclen;
    if (ptr->dstlen > max_dst_repl) max_dst_repl = ptr->dstlen;
  }
}

static buffer* new_buffer(ssize_t size, const char* data)
{
  buffer* newbuf = malloc(sizeof(buffer));
  newbuf->len = size;
  newbuf->buf = malloc(size);
  newbuf->next = 0;
  if (data)
    memcpy(newbuf->buf, data, size);
  return newbuf;
}

static buffer* add_buffer(buffer* newbuf, buffer_list* list)
{
  if(list->tail)
    list->tail->next = newbuf;
  else
    list->head = newbuf;
  list->tail = newbuf;
  return newbuf;
}

static void free_buffer(buffer* buf)
{
  free(buf->buf);
  free(buf);
}

static void free_buffers(buffer_list* list)
{
  buffer* curr;
  buffer* next;
  for (curr = list->head; curr; curr = next) {
    next = curr->next;
    free_buffer(curr);
    curr = next;
  }
  list->head = list->tail = 0;
}

static buffer* merge_buffers(buffer_list* list)
{
  buffer* newbuf;
  size_t total;
  char* ptr;
  buffer* buf;

  total = 0;
  for (buf = list->head; buf; buf = buf->next)
    total += buf->len;
  newbuf = new_buffer(total, 0);

  for (ptr=newbuf->buf, buf=list->head; buf; ptr+=buf->len, buf=buf->next)
    memcpy(ptr, buf->buf, buf->len);

  return newbuf;
}

static void parse_header(buffer_list* buffers)
{
  size_t left;
  char* data;
  
  header = merge_buffers(buffers);

  left = header->len;
  data = header->buf;
  content_html = 0;
  content_length = content_length_offset = content_length_end = 0;
  while(left > 0) {
    char* nl = memchr(data, '\n', left);
    if(!nl) break;
    if(!strncasecmp(data, "Content-Length:", 15)) {
      const char* ptr = data + 15;
      char* tmp;
      while(isspace(*ptr)) ++ptr;
      content_length = strtoul(ptr, &tmp, 10);
      if(!isspace(*tmp))
	content_length = 0;
      else {
	content_length_offset = ptr - header->buf;
	content_length_end = tmp - header->buf;
      }
      content_left = content_length;
    }
    else if(!strncasecmp(data, "Content-Type:", 13)) {
      const char* ptr = data + 13;
      while(isspace(*ptr)) ++ptr;
      if(!strncasecmp(ptr, "text/html", 9) && isspace(ptr[9]))
	content_html = 1;
    }
    left -= nl + 1 - data;
    data = nl + 1;
  }
  /* DEBUG2("content_html=%d content_length=%ld\n",
     content_html, content_length); */
}

static char* find_replacement(char* ptr, size_t left, replacement** repl)
{
  replacement* r;
  char* tmp;
  char* end;
  char* found;
  
  for (end = ptr+left, found = 0, *repl = 0, r = repls; r->src; r++) {
    if ((tmp = strstr(ptr, r->src)) == 0 || tmp > end) continue;
    if (!*repl || tmp < found) {
      found = tmp;
      *repl = r;
    }
  }
  return found;
}

static buffer* xlate_buffer(buffer* in)
{
  size_t diff;
  char* ptr;
  size_t left;
  buffer* out;
  char* found;
  replacement* repl;
  
  out = new_buffer(in->len * max_dst_repl / min_src_repl, 0);
  out->len = 0;
  ptr = in->buf;
  left = in->len;
  while (left > 0) {
    found = find_replacement(ptr, left, &repl);
    if(!found) break;
    diff = found - ptr;

    memcpy(out->buf+out->len, ptr, diff);
    out->len += diff;
    memcpy(out->buf+out->len, repl->dst, repl->dstlen);
    out->len += repl->dstlen;
    
    ptr += diff + repl->srclen;
    left -= diff + repl->srclen;
  }
  if(left > 0) {
    memcpy(out->buf+out->len, ptr, left);
    out->len += left;
  }
  return out;
}

static void write_response(buffer* header, buffer* content)
{
  static char buf[30];
  char* ptr;
  size_t i;
  
  if (content_length_offset) {
    for (i = content->len, ptr = buf+29; i; i /= 10)
      *--ptr = (i % 10) + '0';
    buf[29] = 0;
    write_client(header->buf, content_length_offset);
    write_client(ptr, buf+30-ptr);
    write_client(header->buf + content_length_end,
		 header->len - content_length_end);
  }
  else
    write_client(header->buf, header->len);
  write_client(content->buf, content->len);
}

static void parse_content(buffer_list* buffers)
{
  buffer* content;
  buffer* xlated;

  content = merge_buffers(buffers);
  xlated = xlate_buffer(content);
  free_buffer(content);
  write_response(header, xlated);
  free_buffer(xlated);
}

static void filter_server_data(char* data, ssize_t size)
{
  const char* ptr;
  size_t left;
  size_t used;
  const char* tmp;
  
  for (used=0, ptr=data, left=size; left; left-=used, ptr+=used) {
    DEBUG4("state=%d left=%u *ptr=%u cleft=%u", state, left,
	   *(unsigned char*)ptr, content_left);
    used = 1;			/* Use up one byte by default */
    switch (state) {
    case 0:			/* In header, looing for CR */
      tmp = memchr(ptr, CR, left);
      if (tmp) {
	used = tmp - ptr + 1;
	state = 1;
      }
      else
	used = left;
      break;
    case 1:			/* Read first CR, looking for first LF */
      state = (*ptr == LF) ? 2 : 0;
      break;
    case 2:			/* Read first LF, looking for second CR */
      state = (*ptr == CR) ? 3 : 0;
      break;
    case 3:			/* Read second CR, looking for second LF */
      if (*ptr == LF) {
	add_buffer(new_buffer(ptr-data+1, data), &buffers);
	size -= ptr - data + 1;
	data += ptr - data + 1;
	parse_header(&buffers);
	free_buffers(&buffers);
	state = 4;
      }
      else
	state = 0;
      break;
    case 4:			/* Read second LF, reading body */
      if (content_length && left >= content_left) {
	add_buffer(new_buffer(content_left, data), &buffers);
	used = content_left;
	size -= content_left;
	data += content_left;
	content_left = 0;
	parse_content(&buffers);
	free_buffers(&buffers);
	state = 0;
      }
      else
	used = left;
      break;
    }
    DEBUG2("used=%u next=%d", used, state);
  }
  /* Add any remaining data to the buffer chain before returning */
  if (size) {
    add_buffer(new_buffer(size, data), &buffers);
    if (state == 4) content_left -= size;
  }
}

static void filter_server_eof(void)
{
  if (!content_length && state == 4) {
    parse_content(&buffers);
  }
  exit(0);
}

void filter_init(int argc, char** argv)
{
  int i;
  if (!argc)
    usage("Too few arguments.");
  if (argc % 2)
    usage("Arguments must be paired.");
  repls = malloc((argc/2 + 1) * sizeof(replacement));
  for (i = 0; i < argc/2; i++) {
    repls[i].src = argv[i*2];
    repls[i].dst = argv[i*2+1];
  }
  repls[i].src = repls[i].dst = 0;
  add_filter(CLIENT_IN, (filter_fn)write_server, 0);
  add_filter(SERVER_FD, filter_server_data, filter_server_eof);
  calc_lengths();
}

void filter_deinit(void)
{
}
