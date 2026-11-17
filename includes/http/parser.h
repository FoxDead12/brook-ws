#ifndef PARSER_H
#define PARSER_H
#include "http/http.h"
#define BIT_AT(a, i) (!!((unsigned int)(a)[(unsigned int)(i) >> 3] & (1 << ((unsigned int)(i) & 7))))
#define IS_URL_CHAR(c) (BIT_AT(url_chars, (unsigned char)c))
#define IS_NUM(c) ((c) >= '0' && (c) <= '9')
#define IS_HEX(c) (IS_NUM(c) || (((c) | 0x20) >= 'a' && ((c) | 0x20) <= 'f'))
#define IS_BASE64(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || ((c) >= '0' && (c) <= '9') || ((c) == '+') || ((c) == '/'))
#define IS_HEADER_VALUE_CHAR(c) ((c) == '\t' || ((c) >= 0x20 && (c) != 0x7f))
#define HEADER(s) {s, sizeof(s) - 1}
#define PARSER_CTX(http)                   \
  http_request_t *req = &(http)->req;      \
  http_parser_t *parser = &(http)->parser; \
  buffer_t *buffer = parser->cursor;       \
  size_t index = parser->cursor_offset

/*
 * All "private" functions
 * */
int _http_parser_on_char(http_t *http);
int _http_parser_method(http_t *http);
int _http_parser_url(http_t *http);
int _http_parser_params(http_t *http);
int _http_parser_minor(http_t *http);
int _http_parser_header_field(http_t *http);
int _http_parser_header_value(http_t *http);
int _http_parser_body(http_t *http);

/*
 * All "public" functions avaliable to project
 * */
int http_parser_on_data(http_t *http);

/* clears the request and the parser state, keeps the cursor (keep-alive: next request) */
void http_parser_reset(http_t *http);

#endif // !PARSER_H
