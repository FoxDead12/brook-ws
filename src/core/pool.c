#include "core/pool.h"
#include "core/globals.h"
#include "core/types.h"
#include <stdlib.h>

int brook_pool_init(brook_pool_t *pool) {

  // Initiate the struct of pool in memory
  pool = malloc(sizeof(brook_pool_t));

  if (!pool) {
    return BROOK_ERROR;
  }

  // Now allocate the pool buffer/memory
  int size = BROOK_POOL_BLOCK_SIZE * BROOK_POOL_BLOCKS_AMOUNT;
  pool->data = malloc(size);
  pool->size = size;
  pool->next = NULL;

  if (!pool->data) {
    return BROOK_ERROR;
  }

  // Initiate the buffers
  for (int b = 0; b < BROOK_POOL_BLOCKS_AMOUNT; b++) {

    brook_buffer_t *current = pool->free;

    pool->free = malloc(sizeof(brook_buffer_t));
    pool->free->data = &pool->data[b * BROOK_POOL_BLOCKS_AMOUNT];
    pool->free->size = BROOK_POOL_BLOCK_SIZE;
  }

  return BROOK_OK;
}
