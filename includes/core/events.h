#ifndef EVENTS_H
#define EVENTS_H
#include "core/connection.h"
#include <stddef.h>

typedef struct {
  connection_t *con;
} event_t;

/*
 * Whe fire a event will execute some code
 * this will cleaver the abstraction layer
 */
typedef struct {
  int (*on_data)(void *ctx);
  int (*on_write)(void *ctx);
  int (*on_close)(void *ctx);
} event_handler_t;

/*
 * Will use a chain of events handler
 */
typedef struct {
  const char *name;
  event_handler_t *handlers;
  size_t len;
} pipeline_t;

#endif // EVENTS_H
