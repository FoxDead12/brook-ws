#include "core/pool.h"
#include "core/config.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/types.h"
#include <assert.h>
#include <stdlib.h>

/*
 * Method to initate a pool of process/pointer
 */
int brook_pool_init_head(brook_pool_t **head) {
  brook_pool_t *pool = brook_pool_create();
  if (pool == NULL) {
    return BROOK_ERROR;
  }

  pool->free = pool->nodes;
  *head = pool;
  return BROOK_OK;
}

/*
 * Method to create a pool in memory
 */
brook_pool_t *brook_pool_create(void) {
  size_t size = (size_t)BROOK_POOL_BLOCK_SIZE * BROOK_POOL_BLOCKS_AMOUNT;

  brook_pool_t *pool = calloc(1, sizeof(brook_pool_t));
  if (pool == NULL) {
    return NULL;
  }

  pool->memory = malloc(size);
  if (pool->memory == NULL) {
    free(pool);
    return NULL;
  }

  pool->nodes = calloc(BROOK_POOL_BLOCKS_AMOUNT, sizeof(brook_buffer_t));
  if (pool->nodes == NULL) {
    free(pool->memory);
    free(pool);
    return NULL;
  }

  pool->size = size;
  brook_debug("pool base=%p size=%zu\n", (void *)pool->memory, pool->size);

  for (int b = 0; b < BROOK_POOL_BLOCKS_AMOUNT; b++) {
    brook_buffer_t *node = &pool->nodes[b];
    node->data = &pool->memory[(size_t)b * BROOK_POOL_BLOCK_SIZE];
    node->size = BROOK_POOL_BLOCK_SIZE;
    node->free = BROOK_POOL_BLOCK_SIZE;
    node->next = (b + 1 < BROOK_POOL_BLOCKS_AMOUNT) ? &pool->nodes[b + 1] : NULL;
    brook_debug("pool buffer base=%p size=%d start=%td end=%td\n", (void *)node->data, node->size, node->data - pool->memory, node->data - pool->memory + node->size - 1);
  }

  return pool;
}

int pool_grow(brook_pool_t *head) {
  brook_pool_t *chunk = brook_pool_create();
  if (chunk == NULL) {
    return BROOK_ERROR;
  }

  // Chain the new chunk right after the head (O(1), no walk to the tail)
  chunk->next = head->next;
  head->next = chunk;

  // Splice the new nodes in front of whatever is still free
  chunk->nodes[BROOK_POOL_BLOCKS_AMOUNT - 1].next = head->free;
  head->free = chunk->nodes;

  return BROOK_OK;
}

/*
 * Will return a buffer and remove temporary from free list
 */
brook_buffer_t *brook_pool_malloc(brook_pool_t *head) {

  // Check if the pool contain free slots
  if (head->free == NULL) {
    if (pool_grow(head) == BROOK_ERROR) {
      return NULL;
    }
  }

  // Remove a free slot from chain
  brook_buffer_t *node = head->free;
  assert(node->in_use == 0);

  head->free = node->next;

  // Reset the node
  node->len = 0;
  node->free = node->size;
  node->next = NULL;
  node->in_use = 1;

  brook_debug("pool malloc node=%p base=%p size=%d\n", (void *)node, (void *)node->data, node->size);

  return node;
}

/*
 * Will give a detached buffer back to the free list
 */
int brook_pool_free(brook_pool_t *head, brook_buffer_t *node) {
  assert(node != NULL && node->next == NULL);
  assert(node->in_use == 1);
  node->next = head->free;
  head->free = node;
  node->in_use = 0;

  brook_debug("pool free node=%p base=%p size=%d\n", (void *)node, (void *)node->data, node->size);

  return BROOK_OK;
}
