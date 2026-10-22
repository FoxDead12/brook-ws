#ifndef BROOK_POOL_H
#define BROOK_POOL_H

#include <stddef.h>

#define BROOK_POOL_BLOCK_SIZE 4096
#define BROOK_POOL_BLOCKS_AMOUNT 256

typedef struct brook_buffer_s {
  int size;
  int len;
  int free;
  unsigned char *data;
  struct brook_buffer_s *next;
  int in_use;
} brook_buffer_t;

typedef struct brook_pool_s {
  size_t size;           // total bytes of memory
  unsigned char *memory; // one big region, the raw data of all blocks
  brook_buffer_t *nodes; // block descriptors, allocated once (never freed individually)
  brook_buffer_t *free;  // top of the free list, changes on every get/put
} brook_pool_t;

int brook_pool_init(brook_pool_t **pool_out);
brook_buffer_t *brook_pool_malloc(brook_pool_t *pool);
int brook_pool_free(brook_pool_t *pool, brook_buffer_t *node);

#endif
