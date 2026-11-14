#ifndef BUFFER_H
#define BUFFER_H
#include "core/pool.h"
#include <stddef.h>
#define BUFFER_SIZE 4096

typedef struct buffer_s {
  struct buffer_s *next;
  int len; // total bytes in use
  int pos; // total bytes out/write
  char data[BUFFER_SIZE];
} buffer_t;

typedef struct {
  pool_t *pool;   // pool where get buffers spaces
  buffer_t *head; // the buffer in head of list will be used to start consume/read from memory
  buffer_t *tail; // the last buffer where will write in memory
  size_t total;   // total bytes
  size_t count;   // number of buffers
} chain_t;

#endif // BUFFER_H
