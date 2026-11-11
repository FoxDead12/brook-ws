#ifndef CONNECTION_H
#define CONNECTION_H
#include "core/poll.h"
#include <arpa/inet.h>
#include <stddef.h>
#define MAX_CONNS POLL_MAX_FD

/*
 * This is the callbacks this layer will call to layer above run
 * we now the above layer is the protocol
 */
typedef struct {
  int (*on_open)(void *ctx);
  int (*on_close)(void *ctx);
} connection_protocoll_t;

/*
 * The struct of a connection
 */
typedef struct connection_s {

  // the information the lower level transmit
  int poll_ctx;

  int fd;
  int port;
  char ip[INET_ADDRSTRLEN];

  // callbacks to above layer
  connection_protocoll_t protocol;

} connection_t;

/*
 * Struct will store all existent connections in process/worker
 */
typedef struct {
  connection_t connections[MAX_CONNS];
  connection_t *free[MAX_CONNS];
  size_t free_count;
} connection_manager_t;

/*
 * All "private" functions
 * */

/*
 * All "public" functions avaliable to project
 * */

int connection_setup();
int connection_accept(void *ctx);

int connection_on_read(void *ctx);
int connection_on_write(void *ctx);
int connection_on_close(void *ctx);

#endif // !CONNECTION_H
