#include "http/parser.h"
#include "core/buffer.h"
#include "core/connection.h"
#include "core/logger.h"
#include "core/pool.h"
#include "core/types.h"
#include "http/http.h"
#include <stddef.h>
#include <stdio.h>

/* Tokens as defined by rfc 2616. Also lowercases them.
 *        token       = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */
static const char tokens[256] = {
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0,
    '!',
    0,
    '#',
    '$',
    '%',
    '&',
    '\'',
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    0,
    0,
    '*',
    '+',
    0,
    '-',
    '.',
    0,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    '8',
    '9',
    0,
    0,
    0,
    0,
    0,
    0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    0,
    'a',
    'b',
    'c',
    'd',
    'e',
    'f',
    'g',
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    'h',
    'i',
    'j',
    'k',
    'l',
    'm',
    'n',
    'o',
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    'p',
    'q',
    'r',
    's',
    't',
    'u',
    'v',
    'w',
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    'x',
    'y',
    'z',
    0,
    0,
    0,
    '^',
    '_',
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    '`',
    'a',
    'b',
    'c',
    'd',
    'e',
    'f',
    'g',
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    'h',
    'i',
    'j',
    'k',
    'l',
    'm',
    'n',
    'o',
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    'p',
    'q',
    'r',
    's',
    't',
    'u',
    'v',
    'w',
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    'x',
    'y',
    'z',
    0,
    '|',
    0,
    '~',
    0,
};

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
static const char http_minor[] = "HTTP/1.1";
static const char *methods[] = {
    [DELETE] = "DELETE",
    [GET] = "GET",
    [POST] = "POST",
    [PUT] = "PUT",
    [PATCH] = "PATCH",
};
static const struct {
  const char *name;
  uint8_t len;
} header_names[] = {
    HEADER("host"),
    HEADER("connection"),
    HEADER("content-length"),
    HEADER("content_type"),
    HEADER("content_range"),
    HEADER("user_agent"),
};

/*
 * Will get data from connection rbuffer to parse the HTTP request
 * */
int http_parser_on_data(http_t *http) {

  http_parser_t *parser = &http->parser;
  connection_t *con = http->con;

  // if the cursor buffer is NULL is because is the first reading of connection
  // so we need make the cursor point to current buffer of chain we are parsing normaly the last
  logger_debug("http_parser_on_data: start (cursor=%p, cursor_offset=%zu)", (void *)parser->cursor, (size_t)parser->cursor_offset);

  if (parser->cursor == NULL) {
    logger_debug("http_parser_on_data: first read, cursor -> rbuffer.head");
    parser->cursor = con->rbuffer.head;

    if (parser->cursor == NULL) {
      return BROOK_OK;
    }

    parser->cursor_offset = parser->cursor->pos;
  }

  // walk the chain from the cursor until the last buffer
  while (parser->cursor) {
    buffer_t *b = parser->cursor;

    logger_debug("http_parser_on_data: parsing buffer=%p range [%zu, %zu) (pos=%zu)", (void *)b, parser->cursor_offset, (size_t)b->len, (size_t)b->pos);

    for (size_t i = parser->cursor_offset; i < (size_t)b->len; i++) {
      int rc = _http_parser_on_char(http);
      if (rc != BROOK_HTTP_OK)
        return rc;

      parser->cursor_offset++;

      if (parser->state == s_req_done)
        return BROOK_DONE;
    }

    // last buffer: wait for more data
    if (b->next == NULL)
      break;

    parser->cursor = b->next;
    parser->cursor_offset = parser->cursor->pos;
  }

  logger_debug("http_parser_on_data: end (cursor_offset=%zu)", (size_t)parser->cursor_offset);

  return BROOK_OK;
}

/*
 * Catch char by char in this method
 */
int _http_parser_on_char(http_t *http) {

  http_parser_t *parser = &http->parser;
  connection_t *con = http->con;

  buffer_t *buffer = parser->cursor;
  size_t index = parser->cursor_offset;

  // get the current active char
  unsigned char c = buffer->data[index];

  // handle the state of parcer
  switch (parser->state) {
  case s_req_start:
  case s_req_method:
    return _http_parser_method(http);
  case s_req_url:
    return _http_parser_url(http);
  case s_req_params:
    return _http_parser_params(http);
  case s_req_minor:
    return _http_parser_minor(http);
  case s_req_header_field:
  case s_req_header_value:
  case s_req_headers_done:
  case s_req_body:
  case s_req_done:
    break;
  }

  return BROOK_OK;
}

int _http_parser_method(http_t *http) {

  PARSER_CTX(http);
  unsigned char ch = buffer->data[index];

  // first letter: 'P' is POST, PUT or PATCH, the second letter decides
  if (parser->state == s_req_start) {
    switch (ch) {
    case 'G':
      req->method = GET;
      break;
    case 'P':
      req->method = POST;
      break;
    case 'D':
      req->method = DELETE;
      break;
    default:
      goto bad;
    }
    parser->state = s_req_method;
    parser->match_index = 1;
    return BROOK_HTTP_OK;
  }

  if (req->method == POST && parser->match_index == 1) {
    if (ch == 'U')
      req->method = PUT;
    else if (ch == 'A')
      req->method = PATCH;
  }

  // the rest must match the literal; its '\0' means the name ended and a space must follow
  unsigned char expected = (unsigned char)methods[req->method][parser->match_index];

  if (expected == '\0') {
    if (ch != ' ')
      goto bad;
    parser->state = s_req_url;
    parser->match_index = 0;
    return BROOK_HTTP_OK;
  }

  if (ch != expected)
    goto bad;

  parser->match_index++;
  return BROOK_HTTP_OK;

bad:
  logger_debug("http method error: byte 0x%02x at position %u", ch, (unsigned)parser->match_index);
  return BROOK_HTTP_ERR_BAD_METHOD;
}

/*
 * The url is stored as a reference (buffer, start, len) and url.len is also the
 * match counter: 0 means we are at the first char. '?' and ' ' end the path.
 */
int _http_parser_url(http_t *http) {

  PARSER_CTX(http);

  unsigned char ch = buffer->data[index];
  str_buffer_t *url = &req->url;

  if (url->len == 0) {
    if (ch != '/')
      goto bad;
    url->b = buffer;
    url->start = index;
  } else if (ch == ' ' || ch == '?') {
    parser->state = (ch == ' ') ? s_req_minor : s_req_params;
    return BROOK_HTTP_OK;
  } else if (url->b != buffer) {
    // str_buffer_t references a single buffer, so the url can't cross to the next one
    return BROOK_HTTP_ERR_URL_TOO_LONG;
  }

  if (!IS_URL_CHAR(ch))
    goto bad;

  url->len++;
  return BROOK_HTTP_OK;

bad:
  logger_debug("http url error: invalid byte 0x%02x at position %u", ch, url->len);
  return BROOK_HTTP_ERR_BAD_URL;
}

/*
 * Validates the query string as key=value pairs separated by '&' and stores it raw
 * as a reference. params.len is the char counter (0 = first char of the query).
 * sub_state: 0 key start, 1 in key, 2 after '=', 3 in value. Only ' ' ends the query.
 */
int _http_parser_params(http_t *http) {

  PARSER_CTX(http);

  unsigned char ch = buffer->data[index];
  str_buffer_t *q = &req->params;
  unsigned char sub = parser->sub_state;

  if (q->len == 0) {
    q->b = buffer;
    q->start = index;
    sub = 0;
  } else if (ch != ' ' && q->b != buffer) {
    // str_buffer_t references a single buffer, so the query can't cross to the next one
    return BROOK_HTTP_ERR_URL_TOO_LONG;
  }

  switch (ch) {
  case '=':
    if (sub != 1)
      goto bad;
    sub = 2;
    break;
  case '&':
    if (sub != 3)
      goto bad;
    sub = 0;
    break;
  case ' ':
    if (sub != 3)
      goto bad;
    parser->state = s_req_minor;
    parser->sub_state = 0;
    return BROOK_HTTP_OK;
  default:
    if (!IS_URL_CHAR(ch))
      goto bad;
    if (sub == 0)
      sub = 1;
    else if (sub == 2)
      sub = 3;
  }

  parser->sub_state = sub;
  q->len++;
  return BROOK_HTTP_OK;

bad:
  logger_debug("http params error: byte 0x%02x at position %u (state %u)", ch, q->len, (unsigned)sub);
  return BROOK_HTTP_ERR_BAD_PARAMS;
}

/*
 * Only "HTTP/1.1" followed by "\r\n" is accepted. match_index walks the literal,
 * then sub_state walks the line end (0 expects '\r', 1 expects '\n').
 */
int _http_parser_minor(http_t *http) {

  PARSER_CTX(http);

  unsigned char ch = buffer->data[index];

  if (parser->match_index < sizeof(http_minor) - 1) {
    if (ch != (unsigned char)http_minor[parser->match_index]) {
      logger_debug("http version error: byte 0x%02x at position %u is not %s", ch, (unsigned)parser->match_index, http_minor);
      return BROOK_HTTP_ERR_BAD_VERSION;
    }
    parser->match_index++;
    return BROOK_HTTP_OK;
  }

  if (ch != "\r\n"[parser->sub_state]) {
    logger_debug("http version error: bad line end, got 0x%02x", ch);
    return BROOK_HTTP_ERR_BAD_LINE_END;
  }

  if (++parser->sub_state == 2) {
    parser->state = s_req_header_field;
    parser->match_index = 0;
    parser->sub_state = 0;
  }

  return BROOK_HTTP_OK;
}
