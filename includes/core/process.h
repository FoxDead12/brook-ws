#ifndef BROOK_PROCESS_H
#define BROOK_PROCESS_H

#include "core/connection.h"
#include "core/poll.h"
#include "core/pool.h"
#include <limits.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/resource.h>

#define POLL_FDS_MAX 1024

typedef struct {
  int pid;
  int socket_fd;
  brook_pool_t *pool_head;

  // Variables used only in poll.c
  int nfds;
  struct pollfd fds[POLL_FDS_MAX];
  brook_handler_t handlers[POLL_FDS_MAX];

  // Variables used only in connection.c
  brook_connection_t conns[POLL_FDS_MAX];

} brook_process_t;

int brook_process(void);

#endif
