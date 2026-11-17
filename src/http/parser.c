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

/* Bitmap (bit i of byte i>>3) of the chars allowed raw in the request-target, RFC 3986:
 *   unreserved (ALPHA DIGIT - . _ ~) / sub-delims (! $ & ' ( ) * + , ; =) / ":" / "@" / "/"
 * '%' (needs 2 hex digits) and '?' (query only) are handled by the parser. */
static const uint8_t url_chars[32] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0xd2,
    0xff,
    0xff,
    0x2f,
    0xff,
    0xff,
    0xff,
    0x87,
    0xfe,
    0xff,
    0xff,
    0x47,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
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
    [h_host] = HEADER("host"),
    [h_connection] = HEADER("connection"),
    [h_content_length] = HEADER("content-length"),
    [h_content_type] = HEADER("content-type"),
    [h_content_range] = HEADER("content-range"),
    [h_user_agent] = HEADER("user-agent"),
    [h_transfer_encoding] = HEADER("transfer-encoding"),
};

// every known header needs a name, and one bit of the 32-bit masks
_Static_assert(sizeof(header_names) / sizeof(header_names[0]) == h_general, "header_names must match http_header_e");
_Static_assert(h_general <= 32, "header masks are 32 bits");

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
      http->req.bytes++;

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
    return _http_parser_header_field(http);
  case s_req_header_value:
    return _http_parser_header_value(http);
  case s_req_body:
    return _http_parser_body(http);
  case s_req_done:
    // the request is finished: the caller has to reset the parser before feeding more bytes
    return BROOK_DONE;
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
 * Checks one char of the request-target (RFC 3986): a pchar or "/", or a pct-encoded
 * "%XX". sub_state counts the hex digits still owed by a '%' (0 = not inside one).
 */
static int uri_char(http_parser_t *parser, unsigned char ch) {
  if (parser->sub_state) {
    parser->sub_state--;
    return IS_HEX(ch);
  }
  if (ch == '%') {
    parser->sub_state = 2;
    return 1;
  }
  return IS_URL_CHAR(ch);
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
  } else if (parser->sub_state == 0 && (ch == ' ' || ch == '?')) {
    parser->state = (ch == ' ') ? s_req_minor : s_req_params;
    return BROOK_HTTP_OK;
  }

  if (!uri_char(parser, ch))
    goto bad;

  url->len++;
  return BROOK_HTTP_OK;

bad:
  logger_debug("http url error: invalid byte 0x%02x at position %u", ch, url->len);
  return BROOK_HTTP_ERR_BAD_URL;
}

/*
 * Validates the query (RFC 3986: *( pchar / "/" / "?" ), pct-encoding checked) and
 * stores it raw as a reference. Its key=value structure is not part of the RFC, so it
 * is left to whoever reads it. params.len is the char counter (0 = no query).
 */
int _http_parser_params(http_t *http) {

  PARSER_CTX(http);

  unsigned char ch = buffer->data[index];
  str_buffer_t *q = &req->params;

  // the space ends the query (an empty query is valid, it just leaves params.len == 0)
  if (parser->sub_state == 0 && ch == ' ') {
    parser->state = s_req_minor;
    return BROOK_HTTP_OK;
  }

  if (q->len == 0) {
    q->b = buffer;
    q->start = index;
  }

  // query = *( pchar / "/" / "?" ), so '?' is the one extra char over the path
  if (!(parser->sub_state == 0 && ch == '?') && !uri_char(parser, ch))
    goto bad;

  q->len++;
  return BROOK_HTTP_OK;

bad:
  logger_debug("http params error: invalid byte 0x%02x at position %u", ch, q->len);
  return BROOK_HTTP_ERR_BAD_PARAMS;
}

/*
 * Only "HTTP/1.1" followed by "\r\n" is accepted. match_index walks the literal,
 * then sub_state walks the line end (0 expects '\r', 1 expects '\n').
 */
int _http_parser_minor(http_t *http) {

  PARSER_CTX(http);
  (void)req;

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

/* Headers that must not repeat: Host (RFC 9112 3.2), Content-Length (9112 6.3) and Content-Type (9110 8.3) */
#define HEADER_SINGLETON ((1u << h_host) | (1u << h_content_length) | (1u << h_content_type))

/*
 * Where the value of a known header is stored as a reference. Headers whose value is
 * only read as a number or a flag (connection, content-length, ...) have no slot.
 */
static str_buffer_t *header_slot(http_headers_in_t *h, unsigned state) {
  switch (state) {
  case h_host:
    return &h->host;
  case h_content_type:
    return &h->content_type;
  case h_content_range:
    return &h->content_range;
  case h_user_agent:
    return &h->user_agent;
  default:
    return NULL;
  }
}

/*
 * Content-Length = 1*DIGIT, OWS allowed around it. match_index counts the digits and ws
 * marks a gap after them, so "1 2" is rejected. The multiplication is checked against
 * size_t overflow, a wrapped length would let a body be read as the next request.
 */
static int parse_content_length(http_parser_t *parser, http_headers_in_t *in, unsigned char ch) {
  if (ch == ' ' || ch == '\t') {
    if (parser->match_index)
      parser->ws = 1;
    return BROOK_HTTP_OK;
  }

  if (!IS_NUM(ch) || parser->ws)
    return BROOK_HTTP_ERR_BAD_CONTENT_LEN;

  size_t d = (size_t)(ch - '0');
  if (in->content_length_n > (SIZE_MAX - d) / 10)
    return BROOK_HTTP_ERR_BAD_CONTENT_LEN;

  in->content_length_n = in->content_length_n * 10 + d;
  if (parser->match_index < 255)
    parser->match_index++;
  return BROOK_HTTP_OK;
}

/* Closes a Connection token: "close" turns keep_alive off, any other token is ignored */
static void connection_token_end(http_parser_t *parser, http_headers_in_t *in) {
  if (parser->header_match_mask && parser->match_index == sizeof("close") - 1)
    in->keep_alive = 0;

  parser->header_match_mask = 1;
  parser->match_index = 0;
  parser->ws = 0;
}

/*
 * Connection = #connection-option, a comma separated list of tokens (case-insensitive).
 * Only "close" matters, since we only speak HTTP/1.1 where keep-alive is the default.
 * header_match_mask is 1 while the current token can still be "close", match_index is
 * its length so far (saturating), ws marks a gap inside it ("close x" is invalid).
 */
static int parse_connection(http_parser_t *parser, http_headers_in_t *in, unsigned char ch) {
  static const char word[] = "close";

  if (ch == ',') {
    connection_token_end(parser, in);
    return BROOK_HTTP_OK;
  }

  if (ch == ' ' || ch == '\t') {
    if (parser->match_index)
      parser->ws = 1;
    return BROOK_HTTP_OK;
  }

  unsigned char c = (unsigned char)tokens[ch];
  if (c == 0 || parser->ws)
    return BROOK_HTTP_ERR_BAD_HEADER;

  if (parser->match_index >= sizeof(word) - 1 || c != (unsigned char)word[parser->match_index])
    parser->header_match_mask = 0;

  if (parser->match_index < 255)
    parser->match_index++;
  return BROOK_HTTP_OK;
}

/* End of a header value: checks the ones that need a complete value */
static int header_value_end(http_parser_t *parser, http_headers_in_t *in) {
  switch (parser->header_state) {
  case h_content_length:
    return parser->match_index ? BROOK_HTTP_OK : BROOK_HTTP_ERR_BAD_CONTENT_LEN;
  case h_connection:
    connection_token_end(parser, in);
    return BROOK_HTTP_OK;
  default:
    return BROOK_HTTP_OK;
  }
}

/*
 * The empty line after the headers: checks the request as a whole and decides what
 * comes next, the body (Content-Length bytes) or nothing (s_req_done).
 */
static int headers_end(http_t *http) {
  http_parser_t *parser = &http->parser;
  http_headers_in_t *in = &http->req.headers_in;
  uint32_t seen = parser->header_seen_mask;

  parser->sub_state = 0;

  // RFC 9112 3.2: an HTTP/1.1 request needs a Host (repeats were already rejected)
  if (!(seen & (1u << h_host)))
    return BROOK_HTTP_ERR_BAD_REQUEST;

  if (seen & (1u << h_transfer_encoding)) {
    // RFC 9112 6.1: Transfer-Encoding together with Content-Length is a smuggling vector
    if (seen & (1u << h_content_length))
      return BROOK_HTTP_ERR_BAD_REQUEST;
    return BROOK_HTTP_ERR_NOT_IMPLEMENTED; // chunked bodies are not supported yet
  }

  if (in->content_length_n > BROOK_HTTP_BODY_MAX)
    return BROOK_HTTP_ERR_BODY_TOO_BIG;

  parser->state = in->content_length_n ? s_req_body : s_req_done;
  return BROOK_HTTP_OK;
}

/*
 * Header field name. One bit per known header in header_match_mask: it starts with all
 * set and every byte that differs from a header's name clears that header's bit. At ':'
 * the bit left whose name has exactly the length read is the header (a bit alone can be
 * only a prefix, e.g. "ho" of "host"). No bit means h_general and the value is ignored.
 * sub_state 1 = '\r' of the empty line (end of headers) already seen.
 */
int _http_parser_header_field(http_t *http) {

  PARSER_CTX(http);

  unsigned char ch = buffer->data[index];

  if (++parser->header_bytes > BROOK_HTTP_HEADER_MAX)
    return BROOK_HTTP_ERR_HEADER_TOO_BIG;

  if (parser->sub_state == 1) {
    if (ch != '\n')
      return BROOK_HTTP_ERR_BAD_LINE_END;
    return headers_end(http);
  }

  if (ch == ':') {
    if (parser->match_index == 0)
      goto bad;

    parser->header_state = h_general;
    for (uint32_t m = parser->header_match_mask; m; m &= m - 1) {
      int i = __builtin_ctz(m);
      if (header_names[i].len == parser->match_index) {
        parser->header_state = (unsigned char)i;
        break;
      }
    }

    if (parser->header_state != h_general) {
      uint32_t bit = 1u << parser->header_state;
      if ((parser->header_seen_mask & bit) & HEADER_SINGLETON) {
        logger_debug("http header error: repeated %s", header_names[parser->header_state].name);
        return BROOK_HTTP_ERR_BAD_HEADER;
      }
      parser->header_seen_mask |= bit;
    }

    // a repeated header that may repeat (user-agent, ...) starts its value over
    str_buffer_t *v = header_slot(&req->headers_in, parser->header_state);
    if (v)
      *v = (str_buffer_t){0};

    parser->ws = 0;
    parser->header_match_mask = 1; // connection: the first token can still be "close"
    parser->state = s_req_header_value;
    parser->match_index = 0;
    return BROOK_HTTP_OK;
  }

  if (ch == '\r' && parser->match_index == 0) {
    parser->sub_state = 1;
    return BROOK_HTTP_OK;
  }

  // field-name is a token: tokens[] is 0 for anything else, and lowercases the rest
  unsigned char c = (unsigned char)tokens[ch];
  if (c == 0)
    goto bad;

  if (parser->match_index == 255)
    return BROOK_HTTP_ERR_HEADER_TOO_BIG;

  if (parser->match_index == 0)
    parser->header_match_mask = (1u << h_general) - 1;

  // names are lowercase and end in '\0', which c (never 0) can't match: a header that
  // ran out of chars is cleared too, so the read never goes past its name
  for (uint32_t m = parser->header_match_mask; m; m &= m - 1) {
    int i = __builtin_ctz(m);
    if ((unsigned char)header_names[i].name[parser->match_index] != c)
      parser->header_match_mask &= ~(1u << i);
  }

  parser->match_index++;
  return BROOK_HTTP_OK;

bad:
  logger_debug("http header error: invalid field name byte 0x%02x at position %u", ch, (unsigned)parser->match_index);
  return BROOK_HTTP_ERR_BAD_HEADER;
}

/*
 * Header value (RFC 9110 5.5: no leading or trailing OWS). Content-Length and Connection
 * are read as a number and a flag. The others with a slot are kept as a reference: start
 * at the first char that is not a space, and len only grows on a char that is not a space,
 * adding the spaces waited in parser->ws (so inner spaces count and trailing ones never do).
 * sub_state 1 = '\r' seen.
 */
int _http_parser_header_value(http_t *http) {

  PARSER_CTX(http);

  unsigned char ch = buffer->data[index];

  if (++parser->header_bytes > BROOK_HTTP_HEADER_MAX)
    return BROOK_HTTP_ERR_HEADER_TOO_BIG;

  if (parser->sub_state == 1) {
    if (ch != '\n')
      return BROOK_HTTP_ERR_BAD_LINE_END;

    int rc = header_value_end(parser, &req->headers_in);
    if (rc != BROOK_HTTP_OK)
      return rc;

    parser->state = s_req_header_field;
    parser->header_state = h_general;
    parser->match_index = 0;
    parser->sub_state = 0;
    return BROOK_HTTP_OK;
  }

  if (ch == '\r') {
    parser->sub_state = 1;
    return BROOK_HTTP_OK;
  }

  if (!IS_HEADER_VALUE_CHAR(ch)) {
    logger_debug("http header error: invalid value byte 0x%02x", ch);
    return BROOK_HTTP_ERR_BAD_HEADER;
  }

  switch (parser->header_state) {
  case h_content_length:
    return parse_content_length(parser, &req->headers_in, ch);
  case h_connection:
    return parse_connection(parser, &req->headers_in, ch);
  default:
    break;
  }

  str_buffer_t *v = header_slot(&req->headers_in, parser->header_state);
  if (v == NULL)
    return BROOK_HTTP_OK;

  if (ch == ' ' || ch == '\t') {
    if (v->len)
      parser->ws++;
    return BROOK_HTTP_OK;
  }

  if (v->len == 0) {
    v->b = buffer;
    v->start = index;
  }
  v->len += parser->ws + 1u;
  parser->ws = 0;
  return BROOK_HTTP_OK;
}

/*
 * The body is kept as a reference like the other tokens: body.len counts the bytes read
 * and the request is done when it reaches Content-Length.
 */
int _http_parser_body(http_t *http) {

  PARSER_CTX(http);

  str_buffer_t *body = &req->body;

  if (body->len == 0) {
    body->b = buffer;
    body->start = index;
  }

  if (++body->len == req->headers_in.content_length_n)
    parser->state = s_req_done;

  return BROOK_HTTP_OK;
}

/*
 * Puts the parser and the request in the state of a new request, keeping the cursor so
 * the next request of a keep-alive connection continues where the last one ended.
 * HTTP/1.1 connections are persistent unless a "Connection: close" says otherwise.
 */
void http_parser_reset(http_t *http) {

  buffer_t *cursor = http->parser.cursor;
  size_t offset = http->parser.cursor_offset;

  http->req = (http_request_t){0};
  http->req.headers_in.keep_alive = 1;

  http->parser = (http_parser_t){0};
  http->parser.cursor = cursor;
  http->parser.cursor_offset = offset;
}
