#ifndef BROOK_HTTP_PARSER_H
#define BROOK_HTTP_PARSER_H

#include "core/buffer.h"
#include "http/request.h"
#include <stddef.h>

/*
 * Parse b[0..len) of buffer into req, one byte at a time.
 * Returns BROOK_HTTP_OK or the first error found.
 */
brook_http_err_e brook_http_parse(brook_http_t *req, brook_buffer_t *buffer, unsigned char *b, size_t len);

#endif
