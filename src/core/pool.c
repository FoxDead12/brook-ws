#include "core/pool.h"
#include "core/types.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int pool_alloc(pool_t *pool, size_t size, size_t amount) {

  if (size < sizeof(chunk_t)) {
    // we need this because in memory is necessary contain the chunk struct inside
    size = sizeof(chunk_t);
  }

  // set the values of struct
  *pool = (pool_t){
      .chunk_size = size,
      .chunk_amount = amount,
      .total = 0,
      .free = NULL,
      .chunk = NULL,
  };

  return _pool_grow(pool);
}

int pool_free(pool_t *pool) {

  // dealocate all chains
  chunk_t *chunk = pool->chunk;
  while (chunk != NULL) {
    chunk_t *next = chunk->next;
    free(chunk);
    chunk = next;
  }

  // clean the memory
  memset(pool, 0, sizeof(pool_t));

  return BROOK_OK;
}

int _pool_grow(pool_t *pool) {

  // allocate the new chunk
  chunk_t *new_chunk = malloc(sizeof(chunk_t) + pool->chunk_size * pool->chunk_amount);

  // check if realy OS allocate the memory
  if (new_chunk == NULL) {
    return BROOK_ERROR;
  }

  // for more easy we change the head of the list for the new chunk
  new_chunk->next = pool->chunk;
  pool->chunk = new_chunk;

  // wee need move because the first bytes are reserved for the struct to point to next chunk
  char *free_space = (char *)new_chunk + sizeof(chunk_t);

  // set the new free chunks allocated
  for (size_t i = 0; i < pool->chunk_amount; i++) {
    chunk_t *free_new = (chunk_t *)(free_space + (pool->chunk_size * i));
    free_new->next = pool->free;
    pool->free = free_new;
  }

  // a counter if in need controll for feature
  pool->total += pool->chunk_amount;

  return BROOK_OK;
}

void *pool_acquire(pool_t *pool) {

  // check if contain free chunks to get
  if (pool->free == NULL) {
    // increment the pool chunks
    // TODO: put a limmit of grow i can make
    if (_pool_grow(pool) == BROOK_ERROR) {
      return NULL;
    }
  }

  // remove the header of list
  chunk_t *free = pool->free;
  pool->free = free->next;

  // clean memory is safe fot who will use
  memset(free, 0, pool->chunk_size);

  return free;
}

void pool_release(pool_t *pool, void *ctx) {

  // intrepert was a chunk
  chunk_t *chunk = (chunk_t *)ctx;

  // clean the memory
  memset(chunk, 0, pool->chunk_size);

  chunk->next = pool->free;
  pool->free = chunk;
}
