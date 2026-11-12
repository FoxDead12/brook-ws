#include "core/pool.h"
#include "core/types.h"
#include <stddef.h>
#include <stdlib.h>

int pool_alloc(pool_t *pool, size_t size, size_t amount) {

  if (size <= sizeof(chunk_t)) {
    // we need this because in memory is necessary contain the chunk struct inside
    size = sizeof(chunk_t);
  }

  // set the values of struct
  *pool = (pool_t){
      .chunk_size = size,
      .chunk_amount = amount,
      .free = NULL,
      .chunk = NULL,
  };

  return BROOK_OK;
}

int _pool_grow(pool_t *pool) {

  // allocate the new chunk
  chunk_t *new_chunk = malloc(sizeof(chunk_t) + pool->chunk_size * pool->chunk_amount);

  // for more easy we change the head of the list for the new chunk
  new_chunk->next = pool->chunk;
  pool->chunk = new_chunk;

  // wee need move because the first bytes are reserved for the struct to point to next chunk
  char *free_space = (char *)new_chunk + sizeof(chunk_t);

  // set the new free chunks allocated
  for (int i = 0; i < pool->chunk_amount; i++) {
    chunk_t *free_new = (chunk_t *)(free_space + (pool->chunk_size * i));
    free_new->next = pool->free;
    pool->free = free_new;
  }

  return BROOK_OK;
}

/*
static int pool_grow(pool_t *p) {
  chunk_t *c = malloc(sizeof(chunk_t) + p->obj_size * p->chunk_count);
  if (!c)
    return BROOK_ERROR;

  c->next = p->chunks;
  p->chunks = c;

  char *base = (char *)(c + 1);
  for (size_t i = 0; i < p->chunk_count; i++) {
    free_node_t *n = (free_node_t *)(base + i * p->obj_size);
    n->next = p->free;
    p->free = n;
  }
  return BROOK_OK;
}
*/
