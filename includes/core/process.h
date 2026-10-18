#ifndef BROOK_PROCESS_H
#define BROOK_PROCESS_H

#include <limits.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/resource.h>

#define POLL_FDS_MAX 1024

typedef struct {
  int pid;
  int socket_fd;
  struct pollfd fds[POLL_FDS_MAX];
} brook_process_t;

int brook_process(void);

#endif
