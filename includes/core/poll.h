#ifndef POLL_H
#define POLL_H 
#define POLL_MAX_FD FD_SETSIZE

#include "core/types.h"


/*
 * This is the call backs the polling will call each time a event append
 * is the 1 layer the main of everthing
 */
typedef struct {
  void* ctx;
  void (*on_read)(void* ctx);
  void (*on_open)(void* ctx);
} poll_callbacks_t;





#endif // POLL_H

