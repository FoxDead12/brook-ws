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
} pool_t;

#endif // !POOL_H
