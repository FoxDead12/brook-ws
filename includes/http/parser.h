#ifndef PARSER_H
#define PARSER_H
#include "http/http.h"
#define T(v) (1 << ((v) & 7))
#define BIT_AT(a, i) (!!((unsigned int)(a)[(unsigned int)(i) >> 3] & (1 << ((unsigned int)(i) & 7))))
#define IS_URL_CHAR(c) (BIT_AT(url_chars, (unsigned char)c))
#define IS_NUM(c) ((c) >= '0' && (c) <= '9')
#define IS_BASE64(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || ((c) >= '0' && (c) <= '9') || ((c) == '+') || ((c) == '/'))
#define IS_HEADER_VALUE_CHAR(c) ((c) == '\t' || ((c) >= 0x20 && (c) != 0x7f))
#define HEADER(s) {s, sizeof(s) - 1}

/*
 * All "private" functions
 * */

/*
 * All "public" functions avaliable to project
 * */
int http_parser_on_data(http_t *http);

#endif // !PARSER_H
