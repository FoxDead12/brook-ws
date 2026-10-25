#ifndef BROOK_STR_H
#define BROOK_STR_H

#include "core/buffer.h"
#include <stdio.h>

typedef struct {
  brook_buffer_t *buffer; // block where the string starts
  unsigned short start;   // offset inside that block
  unsigned short len;     // total bytes, can continue in buffer->next
} brook_str_t;

/*
 * Print the string followed by a new line, it follows the blocks when the string continues in the next one
 */
static inline void brook_str_print(const brook_str_t *s) {
  brook_buffer_t *buffer = s->buffer;
  size_t offset = s->start;
  size_t left = s->len;

  while (buffer != NULL && left > 0) {
    // Bytes of this block that belong to the string
    size_t avail = (size_t)buffer->len > offset ? (size_t)buffer->len - offset : 0;
    size_t take = left < avail ? left : avail;

    fwrite(buffer->data + offset, 1, take, stdout);

    left -= take;
    buffer = buffer->next;
    offset = 0;
  }

  fputc('\n', stdout);
}

#endif
