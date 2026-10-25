#ifndef BROOK_HTTP_REQUEST_H
#define BROOK_HTTP_REQUEST_H

#include "core/connection.h"

#define T(v) (1 << ((v) & 7))
#define BIT_AT(a, i) (!!((unsigned int)(a)[(unsigned int)(i) >> 3] & (1 << ((unsigned int)(i) & 7))))
#define IS_URL_CHAR(c) (BIT_AT(normal_url_char, (unsigned char)c))
#define IS_NUM(c) ((c) >= '0' && (c) <= '9')
#define IS_BASE64(C) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '+') || (c == '/'))

typedef enum {
  DELETE,
  GET,
  POST,
  PUT,
  PATCH
} brook_http_method_e;

typedef struct {
  /*
  brook_http_state_e state;
  brook_http_header_state_e header_state;
  unsigned short match_index; // ... position inside a multi byte token
  uint64_t bytes_parsed;
  */
} brook_http_parser_t;

typedef struct {

  brook_http_method_e method;

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
void brook_http_on_data(brook_connection_t *con);

#endif
