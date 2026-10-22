#include "core/pool.h"
#include "core/config.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/types.h"
#include <assert.h>
#include <stdlib.h>

/*
 * Method to create a memory pool
 */
int brook_pool_init(brook_pool_t **pool_out) {

  // Initiate the struct of pool in memory
  brook_pool_t *pool = malloc(sizeof(brook_pool_t));
  if (pool == NULL) {
    return BROOK_ERROR;
  }

  size_t size = (size_t)BROOK_POOL_BLOCK_SIZE * BROOK_POOL_BLOCKS_AMOUNT;

  // One big region, the raw data of all blocks lives here
  pool->memory = malloc(size);
  if (pool->memory == NULL) {
    free(pool);
    return BROOK_ERROR;
  }

  // One descriptor per block, allocated once
  pool->nodes = malloc(sizeof(brook_buffer_t) * BROOK_POOL_BLOCKS_AMOUNT);
  if (pool->nodes == NULL) {
    free(pool->memory);
    free(pool);
    return BROOK_ERROR;
  }

  pool->size = size;

  brook_debug("pool base=%p size=%zu\n", (void *)pool->memory, pool->size);

  // Point each descriptor to its block and chain all of them in the free list
  for (int b = 0; b < BROOK_POOL_BLOCKS_AMOUNT; b++) {
    brook_buffer_t *node = &pool->nodes[b];

    node->data = &pool->memory[(size_t)b * BROOK_POOL_BLOCK_SIZE];
    node->size = BROOK_POOL_BLOCK_SIZE;
    node->len = 0;
    node->free = BROOK_POOL_BLOCK_SIZE;
    node->next = (b + 1 < BROOK_POOL_BLOCKS_AMOUNT) ? &pool->nodes[b + 1] : NULL;
    node->in_use = 0;

    brook_debug("pool buffer base=%p size=%d start=%td end=%td\n", (void *)node->data, node->size, node->data - pool->memory, node->data - pool->memory + node->size - 1);
  }

  // The free list starts at the first node, nodes keeps the base for the free
  pool->free = pool->nodes;

  *pool_out = pool;
  return BROOK_OK;
}

/*
 * Will return a buffer and remove temporary from free list
 */
brook_buffer_t *brook_pool_malloc(brook_pool_t *pool) {

  // Check if the pool contain free slots
  if (pool->free == NULL) {
    brook_debug("FUUUCKKKK NEED ALLOCATE MORE MEMORY\n");
    return NULL;
  }

  // Remove a free slot from chain
  brook_buffer_t *node = pool->free;
  assert(node->in_use == 0);

  pool->free = node->next;

  // Reset the node
  node->len = 0;
  node->free = node->size;
  node->next = NULL;
  node->in_use = 1;

  return node;
}

/*
 * Will give a detached buffer back to the free list
 */
int brook_pool_free(brook_pool_t *pool, brook_buffer_t *node) {
  assert(node != NULL && node->next == NULL);
  assert(node->in_use == 1);
  node->next = pool->free;
  pool->free = node;
  node->in_use = 0;

  return BROOK_OK;
}
