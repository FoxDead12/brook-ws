#ifndef BROOK_CONNECTION_H
#define BROOK_CONNECTION_H

#include "core/pool.h"
#include "core/types.h"
#include <arpa/inet.h>

typedef struct brook_connection_s brook_connection_t;

typedef struct {
  void (*on_open)(brook_connection_t *con);
  void (*on_data)(brook_connection_t *con);
  void (*on_close)(brook_connection_t *con);
} brook_protocol_t;

struct brook_connection_s {
  int fd;
  int slot;
  int port;
  char ip[INET_ADDRSTRLEN];
  brook_buffer_t *buffers;
  brook_protocol_t *protocol;
};

int brook_connection_set_default_protocol(brook_protocol_t *protocol);
int brook_connection_set_protocol(brook_connection_t *con, brook_protocol_t *protocol);

void brook_connection_accept(void *ctx);
void brook_connection_destroy(void *ctx);
void brook_connection_read(void *ctx);

#endif
