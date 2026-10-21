#include "core/pool.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/types.h"
#include <stdlib.h>

int brook_pool_init(brook_pool_t **pool_out) {

  // Initiate the struct of pool in memory
  brook_pool_t *pool = malloc(sizeof(brook_pool_t));

  if (!pool) {
    return BROOK_ERROR;
  }

  // Now allocate the pool buffer/memory
  int size = BROOK_POOL_BLOCK_SIZE * BROOK_POOL_BLOCKS_AMOUNT;
  pool->data = malloc(size);
  pool->size = size;
  pool->free = NULL;

  if (!pool->data) {
    return BROOK_ERROR;
  }

  brook_debug("pool base=%p size=%d\n", (void *)pool->data, pool->size);

  // Initiate the buffers
  for (int b = 0; b < BROOK_POOL_BLOCKS_AMOUNT; b++) {

    brook_buffer_t *current = pool->free;
    pool->free = malloc(sizeof(brook_buffer_t));
    if (!pool->free) {
      return BROOK_ERROR;
    }

    pool->free->data = &pool->data[b * BROOK_POOL_BLOCK_SIZE];
    pool->free->size = BROOK_POOL_BLOCK_SIZE;
    pool->free->len = 0;
    pool->free->next = current;

    // Log the buffer
    brook_debug("pool buffer base=%p size=%d start=%td end=%td\n", (void *)pool->free->data, pool->free->size, pool->free->data - pool->data, pool->free->data - pool->data + pool->free->size - 1);
  }

  *pool_out = pool;
  return BROOK_OK;
}
