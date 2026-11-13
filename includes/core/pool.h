#ifndef POOL_H
#define POOL_H
#include <stddef.h>
#include <stdint.h>

/*
 * A pool struct will allocated amounted of memory of a type of data
 * but is dinamicly is possible add new nodes
 */
typedef struct chunk_s {
  struct chunk_s *next;
} chunk_t;

typedef struct {
  size_t chunk_size;   // the number of types of variable type (int, struct)
  size_t chunk_amount; // the number of chunks/nodes exist
  chunk_t *chunk;      // linked list of chunks allocated in memory
  chunk_t *free;       // a linked list to all free chunks/nodes
  size_t total;
} pool_t;

/*
 * All "private" functions
 * */
int _pool_grow(pool_t *pool);

/*
 * All "public" functions avaliable to project
 * */
int pool_alloc(pool_t *pool, size_t size, size_t amount);
int pool_free(pool_t *pool);

void *pool_acquire(pool_t *pool);
void pool_release(pool_t *pool, void *ctx);

#endif // !POOL_H
