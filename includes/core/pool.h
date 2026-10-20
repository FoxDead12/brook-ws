#ifndef BROOK_POOL_H
#define BROOK_POOL_H

#define BROOK_POOL_BLOCK_SIZE 4096
#define BROOK_POOL_BLOCKS_AMOUNT 256

typedef struct brook_buffer_s {
  int size;
  int len;
  unsigned char *data;
  struct brook_buffer_s *next;
} brook_buffer_t;

typedef struct brook_pool_s {
  int size;
  unsigned char *data;       // the memory allocation
  brook_buffer_t *free;      // the free buffers
  struct brook_pool_s *next; // the next pool if exist
} brook_pool_t;

#endif
