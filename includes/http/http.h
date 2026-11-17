#ifndef HTTP_H
#define HTTP_H
#include "core/buffer.h"
#include "core/connection.h"
#include "core/str.h"
#include <stddef.h>

typedef struct {
  str_buffer_t host;
  str_buffer_t connection;
  str_buffer_t content_length;
  str_buffer_t content_type;
  str_buffer_t content_range;
  str_buffer_t user_agent;

  size_t content_length_n;
  unsigned keep_alive : 1;
} http_headers_in_t;

typedef struct {

  connection_t *con;

  buffer_t *head;
  buffer_t *body;

  http_headers_in_t headers_in;

} http_request_t;

/*
 * All "private" functions
 * */

/*
 * All "public" functions avaliable to project
 * */

#endif // !HTTP_H
