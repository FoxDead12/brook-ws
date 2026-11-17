#ifndef HTTP_H
#define HTTP_H
#include "core/buffer.h"
#include "core/connection.h"
#include "core/str.h"
#include <stddef.h>

typedef struct {
  buffer_t *cursor;
  size_t cursor_offset;
  int state;
} http_parser_t;

typedef enum {
  DELETE,
  GET,
  POST,
  PUT,
  PATCH
} http_method_e;

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
  http_method_e method;
  http_headers_in_t headers_in;
} http_request_t;

typedef struct {
  connection_t *con;
  http_request_t req;
  http_parser_t parser;
} http_t;

/*
 * All "private" functions
 * */

/*
 * All "public" functions avaliable to project
 * */
int http_on_open(connection_t *con);
int http_on_data(connection_t *con);
int http_on_write(connection_t *con);
int http_on_close(connection_t *con);

int http_setup(void);

#endif // !HTTP_H
