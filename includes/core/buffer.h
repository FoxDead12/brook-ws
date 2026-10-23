#ifndef BROOK_BUFFER_H
#define BROOK_BUFFER_H

typedef struct brook_buffer_s {
  int size;
  int len;
  int free;
  unsigned char *data;
  struct brook_buffer_s *next;
  int in_use;
} brook_buffer_t;

#endif
