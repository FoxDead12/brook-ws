#ifndef BROOK_HTTP_REQUEST_H
#define BROOK_HTTP_REQUEST_H

#include "core/buffer.h"
#include "core/connection.h"
#include "core/str.h"
#include <stddef.h>
#include <stdint.h>

#define BROOK_HTTP_HEADER_MAX 8192

/*
 * Every way the request parser can fail. BROOK_HTTP_OK must stay 0.
 * This need be aligned with brook_http_errors[] in request.c
 */
typedef enum {
  BROOK_HTTP_OK = 0,

  BROOK_HTTP_ERR_BAD_REQUEST,     // generic malformed request
  BROOK_HTTP_ERR_BAD_METHOD,      // unknown or unsupported method
  BROOK_HTTP_ERR_BAD_URL,         // url does not start with '/' or has an invalid char
  BROOK_HTTP_ERR_URL_TOO_LONG,    // url bigger than the allowed limit
  BROOK_HTTP_ERR_BAD_PARAMS,      // malformed query string (key=value&...)
  BROOK_HTTP_ERR_BAD_VERSION,     // not HTTP/1.1
  BROOK_HTTP_ERR_BAD_LINE_END,    // '\r' not followed by '\n'
  BROOK_HTTP_ERR_BAD_HEADER,      // empty or invalid header name / value
  BROOK_HTTP_ERR_BAD_CONTENT_LEN, // content-length is not a number
  BROOK_HTTP_ERR_HEADER_TOO_BIG,  // headers bigger than BROOK_HTTP_HEADER_MAX

  BROOK_HTTP_ERR_COUNT
} brook_http_err_e;

typedef struct {
  uint16_t status;
  const char *message;
} brook_http_error_t;

extern const brook_http_error_t brook_http_errors[BROOK_HTTP_ERR_COUNT];

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

  s_req_header_field,
  s_req_header_value,
  s_req_headers_done,

  s_req_body,
  s_req_done

} brook_http_parser_state_e;

typedef enum {
  h_general,
  h_host,
  h_content_length,
  h_connection,
  h_upgrade,
  h_ws_key,
} brook_http_header_e;

typedef struct {

  // Where the current request starts, buffers before this one can be released
  brook_buffer_t *start_buffer;
  int start_offset;

  // Next byte to parse, where the parser resumes
  brook_buffer_t *cursor_buffer;
  int cursor_offset;

  // General porpose
  brook_http_parser_state_e state;
  unsigned int match_index;
  unsigned int sub_state;

  // Related to headers
  unsigned int header_size;
  uint8_t header_match_mask;
  brook_http_header_e header_state;

} brook_http_parser_t;

typedef struct {

  brook_http_parser_t parser; // TODO: delete

  brook_http_method_e method;
  brook_str_t url;
  brook_str_t params;

  size_t content_length;
  unsigned keep_alive : 1;

} brook_http_request_t;

typedef struct {
  brook_http_parser_t parser;
  brook_http_request_t request;
} brook_http_t;

void brook_http_open(brook_connection_t *con);
void brook_http_on_close(brook_connection_t *con);
void brook_http_on_data(brook_connection_t *con, brook_buffer_t *buffer, unsigned char *b, size_t len);
void brook_http_on_drain(brook_connection_t *con);
void brook_http_reset(brook_connection_t *con);

#endif
