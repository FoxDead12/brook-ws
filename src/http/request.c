#include "http/request.h"
#include "core/buffer.h"
#include "core/connection.h"
#include "core/globals.h"
#include "core/pool.h"
#include "core/str.h"
#include "core/types.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Tokens as defined by rfc 2616. Also lowercases them.
 *        token       = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */
static const char tokens[256] = {
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0, '!', 0, '#', '$', '%', '&', '\'',
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    0, 0, '*', '+', 0, '-', '.', 0,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    '0', '1', '2', '3', '4', '5', '6', '7',
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    '8', '9', 0, 0, 0, 0, 0, 0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    0, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    'x', 'y', 'z', 0, 0, 0, '^', '_',
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    'x', 'y', 'z', 0, '|', 0, '~', 0};

static const uint8_t url_chars[32] = {
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0 | T(2) | 0 | 0 | T(16) | 0 | 0 | 0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0 | 2 | 4 | 0 | 16 | 32 | 64 | 128,
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    1 | 2 | 4 | 8 | 16 | 32 | 64 | 0,
};

static const char *methods[] = {
    [DELETE] = "DELETE",
    [GET] = "GET",
    [POST] = "POST",
    [PUT] = "PUT",
    [PATCH] = "PATCH",
};

/*
 * Method to initiate this http protocol in connection
 */
void brook_http_open(brook_connection_t *con) {

  // Allocate a buffer to context of protocol
  brook_buffer_t *memory = brook_buffer_get_free(&con->protocol_ctx);
  if (memory == NULL) {
    brook_log(&logger, L_ERROR, "Can't allocate http context for fd=%d\n", con->fd);
    brook_connection_destroy(con);
    return;
  }

  // We need contain this check before compile
  _Static_assert(sizeof(brook_http_request_t) <= BROOK_POOL_BLOCK_SIZE, "request does not fit in a pool block");

  // Define the struct in memory and clean the bytes
  brook_http_request_t *req = (brook_http_request_t *)(void *)memory->data;
  memset(req, 0, sizeof(*req));
}

/*
 * Method called when connection change protocol or is closed
 */
void brook_http_on_close(brook_connection_t *con) {
  // brook_buffer_t *memory = con->protocol_ctx;
  brook_buffer_destroy(con->protocol_ctx);
  con->protocol_ctx = NULL;
}

/*
 * Method called each time connection read data
 */
void brook_http_on_data(brook_connection_t *con, brook_buffer_t *buffer, unsigned char *b, size_t len) {
  brook_buffer_t *memory = con->protocol_ctx;
  brook_http_request_t *req = (brook_http_request_t *)(void *)memory->data;

  // Parse the received data, TODO: handle error
  brook_http_parse(req, buffer, b, len);

  brook_str_print(&req->url);
  brook_str_print(&req->params);
}

/*
 * Will iterate a buffer char by char to parse the http message
 */
int brook_http_parse(brook_http_request_t *req, brook_buffer_t *buffer, unsigned char *b, size_t len) {
  brook_http_parser_t *parser = &req->parser;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = b[i];
    int rc = 0;

    switch (parser->state) {
    case s_req_start:
    case s_req_method:
      rc = brook_http_parse_method(req, c);
      break;
    case s_req_url:
      rc = brook_http_parse_url(req, buffer, &b[i]);
      break;
    case s_req_params:
      rc = brook_http_parse_params(req, buffer, &b[i]);
      break;
    case s_req_minor:
      rc = brook_http_parse_minor(req, c);
      break;
    }

    if (rc == BROOK_ERROR) {
      brook_debug("http parse error in state=%d match_index=%u at byte=%zu of %zu\n", (int)parser->state, parser->match_index, i, len);
      return BROOK_ERROR;
    }
  }

  return BROOK_OK;
}

/*
 * Will read the method of request one char at a time, it ends when finds the space after the name
 */
int brook_http_parse_method(brook_http_request_t *req, unsigned char ch) {
  // Get the char from tokens chars image is 'G' will transform in 'g'
  unsigned char c = tokens[ch];
  brook_http_parser_t *parser = &req->parser;

  assert(parser->state == s_req_start || parser->state == s_req_method);

  // First letter: P can be POST, PUT or PATCH, it is decided by the second letter
  if (parser->state == s_req_start) {
    switch (c) {
    case 'g':
      req->method = GET;
      break;
    case 'p':
      req->method = POST;
      break;
    case 'd':
      req->method = DELETE;
      break;
    default:
      brook_debug("http method error: invalid first byte 0x%02x\n", ch);
      return BROOK_ERROR;
    }

    parser->state = s_req_method;
    parser->match_index = 1;
    return BROOK_OK;
  }

  int match_index = parser->match_index;

  if (req->method == POST && match_index == 1) {
    if (c == 'u') {
      req->method = PUT;
    } else if (c == 'a') {
      req->method = PATCH;
    }
  }

  // Get the name of method literal string
  unsigned char expected = (unsigned char)methods[req->method][parser->match_index];

  // Check if we are in last character of name
  if (expected == '\0') {
    if (ch != ' ') {
      brook_debug("http method error: %s must be followed by a space, got 0x%02x\n", methods[req->method], ch);
      return BROOK_ERROR;
    }
    parser->state = s_req_url;
    parser->match_index = 0;
    return BROOK_OK;
  }

  // Transform with tokens
  expected = tokens[expected];

  if (c != expected) {
    brook_debug("http method error: byte 0x%02x at position %d does not match %s (expected 0x%02x)\n", ch, match_index, methods[req->method], expected);
    return BROOK_ERROR;
  }

  parser->match_index++;

  return BROOK_OK;
}

int brook_http_parse_url(brook_http_request_t *req, brook_buffer_t *buffer, unsigned char *at) {
  brook_http_parser_t *parser = &req->parser;
  unsigned char ch = *at;

  assert(parser->state == s_req_url);

  // start of url
  if (parser->match_index == 0 && ch != '/') {
    brook_debug("http url error: url must start with '/', got 0x%02x\n", ch);
    return BROOK_ERROR;
  }

  if (parser->match_index == 0) {
    req->url.buffer = buffer;
    req->url.start = (unsigned short)(at - buffer->data);
  }

  // end of url
  if (ch == '?') {
    req->url.len = req->parser.match_index;
    parser->state = s_req_params;
    parser->match_index = 0;
    return BROOK_OK;
  }

  if (ch == ' ') {
    req->url.len = req->parser.match_index;
    parser->state = s_req_minor;
    parser->match_index = 0;
    return BROOK_OK;
  }

  // Check if a valida URL char
  if (!IS_URL_CHAR(ch)) {
    brook_debug("http url error: invalid char 0x%02x at position %u of the url\n", ch, parser->match_index);
    return BROOK_ERROR;
  }

  parser->match_index++;

  return BROOK_OK;
}

int brook_http_parse_params(brook_http_request_t *req, brook_buffer_t *buffer, unsigned char *at) {
  brook_http_parser_t *parser = &req->parser;
  unsigned char ch = *at;

  assert(parser->state == s_req_params);

  if (parser->match_index == 0) {
    req->params.buffer = buffer;
    req->params.start = (unsigned short)(at - buffer->data);
  }

  if (ch == ' ') {
    req->params.len = parser->match_index;
    parser->state = s_req_minor;
    parser->match_index = 0;
    return BROOK_OK;
  }

  if (!IS_URL_CHAR(ch)) {
    brook_debug("http url error: invalid char 0x%02x at position %u of the url\n", ch, parser->match_index);
    return BROOK_ERROR;
  }

  parser->match_index++;

  return BROOK_OK;
}

int brook_http_parse_minor(brook_http_request_t *req, unsigned char ch) {
  return BROOK_OK;
}
