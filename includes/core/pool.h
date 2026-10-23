#ifndef BROOK_POOL_H
#define BROOK_POOL_H

#include "core/buffer.h"
#include <stddef.h>

#define BROOK_POOL_BLOCK_SIZE 4096
#define BROOK_POOL_BLOCKS_AMOUNT 256

typedef struct brook_pool_s {
  size_t size;           // total bytes of memory
  unsigned char *memory; // one big region, the raw data of all blocks
  brook_buffer_t *nodes; // block descriptors, allocated once (never freed individually)
  brook_buffer_t *free;  // top of the free list, changes on every get/put
  struct brook_pool_s *next;
} brook_pool_t;

int brook_pool_init_head(brook_pool_t **head);
brook_pool_t *brook_pool_create(void);
int pool_grow(brook_pool_t *head);
brook_buffer_t *brook_pool_malloc(brook_pool_t *head);
int brook_pool_free(brook_pool_t *head, brook_buffer_t *node);

#endif
