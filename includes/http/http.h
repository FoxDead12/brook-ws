#ifndef HTTP_H
#define HTTP_H
#include "core/buffer.h"
#include "core/connection.h"
#include "core/str.h"
#include <stddef.h>
#include <stdint.h>

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
  BROOK_HTTP_ERR_NOT_IMPLEMENTED, // Transfer-Encoding (chunked bodies) is not supported yet
  BROOK_HTTP_ERR_BODY_TOO_BIG,    // content-length bigger than BROOK_HTTP_BODY_MAX

  BROOK_HTTP_ERR_COUNT
} brook_http_err_e;

typedef enum {
  s_req_start,
  s_req_method,
  s_req_url,
  s_req_params,
  s_req_minor,

  s_req_header_field,
  s_req_header_value,

  s_req_body,
  s_req_done
} http_parser_state_e;

// RFC 9112 3: recipients should support at least 8000 octets in the request line
#define BROOK_HTTP_HEADER_MAX 8192

// the body stays in the chain buffers until the request is handled, so it needs a limit
#define BROOK_HTTP_BODY_MAX (1u << 20)

/*
 * Headers the server cares about. The value is the index in header_names[] and also the
 * bit used in the match/seen masks. h_general is any other header (and the number of
 * known ones), its value is ignored.
 */
typedef enum {
  h_host,
  h_connection,
  h_content_length,
  h_content_type,
  h_content_range,
  h_user_agent,
  h_transfer_encoding,
  h_general
} http_header_e;

typedef struct {
  buffer_t *cursor;
  size_t cursor_offset;
  http_parser_state_e state;
  unsigned char match_index; // chars of the current literal (method, version) already matched
  unsigned char sub_state;   // url/params: hex digits still owed by a '%'; version: line end position

  unsigned char header_state;  // http_header_e of the header whose value is being read
  uint32_t header_match_mask;  // known headers still matching the field name read so far
  uint32_t header_seen_mask;   // known headers already present in this request
  uint16_t header_bytes;       // bytes of the header block read so far, limited by BROOK_HTTP_HEADER_MAX
  uint16_t ws;                 // spaces of a header value waiting to see if the value goes on
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
  str_buffer_t url;
  str_buffer_t params; // raw query string after '?', empty when len == 0
  http_headers_in_t headers_in;
  str_buffer_t body; // Content-Length bytes after the headers, empty when len == 0
  size_t bytes;      // bytes of this request read so far (line + headers + body): what chain_consume frees at the end
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
