#include "core/str.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
 * Next contiguous piece of the token, at most `left` bytes. (*b, *off) is the read
 * position: when this buffer has no more data it moves to the next one.
 * Returns the piece size (0 if the chain ended) and sets *p to its first byte.
 */
static size_t str_chunk(const buffer_t **b, size_t *off, size_t left, const unsigned char **p) {
  while (*b && *off >= (size_t)(*b)->len) {
    *b = (*b)->next;
    if (*b)
      *off = (size_t)(*b)->pos;
  }

  if (*b == NULL)
    return 0;

  size_t n = (size_t)(*b)->len - *off;
  if (n > left)
    n = left;

  *p = (*b)->data + *off;
  *off += n;
  return n;
}

int str_buffer_eq(const str_buffer_t *s, const char *lit, size_t n) {
  if (s->len != n)
    return 0;

  const buffer_t *b = s->b;
  size_t off = s->start;
  size_t left = n;
  const unsigned char *p;

  while (left) {
    size_t c = str_chunk(&b, &off, left, &p);
    if (c == 0 || memcmp(p, lit, c) != 0)
      return 0;

    lit += c;
    left -= c;
  }

  return 1;
}

void str_buffer_print(const str_buffer_t *s) {
  const buffer_t *b = s->b;
  size_t off = s->start;
  size_t left = s->len;
  const unsigned char *p;

  while (left) {
    size_t c = str_chunk(&b, &off, left, &p);
    if (c == 0)
      break;

    fwrite(p, 1, c, stdout);
    left -= c;
  }
}

size_t str_buffer_peek(const str_buffer_t *s, char *dst, size_t cap) {
  size_t done = 0;

  if (cap == 0)
    return 0;

  const buffer_t *b = s->b;
  size_t off = s->start;
  size_t left = s->len;
  const unsigned char *p;

  while (left && done < cap - 1) {
    size_t want = left < cap - 1 - done ? left : cap - 1 - done;
    size_t c = str_chunk(&b, &off, want, &p);
    if (c == 0)
      break;

    memcpy(dst + done, p, c);
    done += c;
    left -= c;
  }

  dst[done] = '\0';
  return done;
}
