#ifndef BROOK_HTTP_REQUEST_H
#define BROOK_HTTP_REQUEST_H

#include "core/connection.h"
#include "core/str.h"

#define T(v) (1 << ((v) & 7))
#define BIT_AT(a, i) (!!((unsigned int)(a)[(unsigned int)(i) >> 3] & (1 << ((unsigned int)(i) & 7))))
#define IS_URL_CHAR(c) (BIT_AT(url_chars, (unsigned char)c))
#define IS_NUM(c) ((c) >= '0' && (c) <= '9')
#define IS_BASE64(C) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '+') || (c == '/'))

typedef enum {
  DELETE,
  GET,
  POST,
  PUT,
  PATCH
} brook_http_method_e;

typedef enum {

  // States to parse first line of HTTP
  s_req_start,
  s_req_method,
  s_req_url,
  s_req_params,
  s_req_minor,

} brook_http_parser_state_e;

typedef struct {
  brook_http_parser_state_e state;
  unsigned int match_index;
  unsigned int sub_state;

  /*
  brook_http_state_e state;
  brook_http_header_state_e header_state;
  unsigned short match_index; // ... position inside a multi byte token
  uint64_t bytes_parsed;
  */
} brook_http_parser_t;

typedef struct {

  brook_http_parser_t parser;

  brook_http_method_e method;
  brook_str_t url;
  brook_str_t params;

  /*
  brook_method_e method;
  unsigned char version_major;
  unsigned char version_minor;

  unsigned keep_alive : 1;
  unsigned upgrade : 1;
  unsigned chunked : 1;

  uint16_t header_count;
  uint64_t content_length;

  brook_str_t url;
  brook_str_t query_string; // ... raw, parsed on demand
  brook_str_t host;
  brook_str_t cookies; // ... raw, parsed on demand
  brook_str_t ws_key;  // ... Sec-WebSocket-Key when upgrade

  */
} brook_http_request_t;

void brook_http_open(brook_connection_t *con);
void brook_http_on_close(brook_connection_t *con);
void brook_http_on_data(brook_connection_t *con, brook_buffer_t *buffer, unsigned char *b, size_t len);

int brook_http_parse(brook_http_request_t *req, brook_buffer_t *buffer, unsigned char *b, size_t len);
int brook_http_parse_method(brook_http_request_t *parser, unsigned char ch);
int brook_http_parse_url(brook_http_request_t *req, brook_buffer_t *buffer, unsigned char *at);
int brook_http_parse_params(brook_http_request_t *req, brook_buffer_t *buffer, unsigned char *at);
int brook_http_parse_minor(brook_http_request_t *req, unsigned char ch);

#endif
