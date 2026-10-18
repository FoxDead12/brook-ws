#ifndef BROOK_CONNECTION_H
#define BROOK_CONNECTION_H

#include "core/types.h"
#include <arpa/inet.h>

typedef struct {
  int fd;
  char ip[INET_ADDRSTRLEN];
  int port;
} brook_connection_t;

void brook_connection_accept(void *ctx);
void brook_connection_read(void *ctx);

#endif
