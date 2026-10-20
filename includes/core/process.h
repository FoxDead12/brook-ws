#ifndef BROOK_PROCESS_H
#define BROOK_PROCESS_H

#include "core/connection.h"
#include "core/pool.h"
#include <limits.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/resource.h>

#define POLL_FDS_MAX 1024

typedef struct {
  void (*on_read)(void *ctx);
  void (*on_write)(void *ctx);
  void (*on_error)(void *ctx); // POLLERR | POLLHUP | POLLNVAL
  void *ctx;
} brook_handler_t;

typedef struct {
  int pid;
  int socket_fd;

  // Sockets active in process
  struct pollfd fds[POLL_FDS_MAX];
  brook_handler_t handlers[POLL_FDS_MAX];
  brook_connection_t conns[POLL_FDS_MAX];
  int nfds;

  brook_pool_t *pool;
} brook_process_t;

int brook_process(void);

#endif
