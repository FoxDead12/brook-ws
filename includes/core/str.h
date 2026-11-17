#ifndef STR_H
#define STR_H

#include "core/buffer.h"
typedef struct {

  buffer_t *b;
  unsigned int start;
  unsigned int len;

} str_buffer_t;

#endif // !STR_H
