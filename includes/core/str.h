#ifndef STR_H
#define STR_H

#include "core/buffer.h"
typedef struct {

  buffer_t *b;
  unsigned int start;
  unsigned int len;

} str_buffer_t;

/*
 * A str_buffer_t is a reference (first buffer, start, total len) to bytes that may span
 * several buffers of a chain. These helpers walk the chain, so no copy is made.
 * They are only valid while the buffers are still in the chain (before chain_consume).
 */

/* 1 if the token has exactly the n bytes of lit, 0 otherwise */
int str_buffer_eq(const str_buffer_t *s, const char *lit, size_t n);

/* eq against a string literal: STR_EQ(&req->url, "/health") */
#define STR_EQ(s, lit) str_buffer_eq((s), (lit), sizeof(lit) - 1)

/* writes the token to stdout, no newline */
void str_buffer_print(const str_buffer_t *s);

/*
 * Copies at most cap-1 bytes of the token to dst ('\0' terminated) and returns how many.
 * Only meant for debug logs (a bounded copy on the caller's stack, the token is not kept).
 */
size_t str_buffer_peek(const str_buffer_t *s, char *dst, size_t cap);

#endif // !STR_H
