#include "core/process.h"
#include "core/config.h"
#include "core/connection.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/pool.h"
#include "core/types.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

/*
 * Register the listening socket as the first slot of the poll set
 */
static void brook_process_register_listener(void) {
  process.fds[0].fd = process.socket_fd;
  process.fds[0].events = POLLIN;
  process.fds[0].revents = 0;

  process.handlers[0] = (brook_handler_t){
      .on_read = brook_connection_accept,
      .on_write = NULL,
      .on_error = NULL,
      .ctx = NULL,
  };
  process.nfds = 1;
}

/*
 * Dispatch the events of one slot to its handler
 */
static void brook_process_dispatch(int slot) {
  short events = process.fds[slot].revents;
  brook_handler_t *handler = &process.handlers[slot];

  brook_debug("event slot=%d fd=%d revents=0x%x\n", slot, process.fds[slot].fd, events);

  // if ((events & (POLLERR | POLLHUP | POLLNVAL)) && handler->on_error) {
  //   handler->on_error(handler->ctx);
  //   return;
  // }

  if ((events & POLLIN) && handler->on_read) {
    handler->on_read(handler->ctx);
  }

  if ((events & POLLIN) && handler->on_write) {
    handler->on_write(handler->ctx);
  }
}

/*
 * Wait for events and dispatch them until poll fails
 */
static void brook_process_loop(void) {
  while (1) {
    int nready = poll(process.fds, process.nfds, -1);
    if (nready == -1) {
      brook_log(&logger, L_ERROR, "Can't execute poll: %s\n", strerror(errno));
      return;
    }
    brook_debug("poll returned nready=%d nfds=%d\n", nready, process.nfds);

    // Stop scanning once every ready fd was handled
    for (int i = 0; i < process.nfds && nready > 0; i++) {
      if (process.fds[i].revents == 0) {
        continue;
      }

      brook_process_dispatch(i);
      nready--;
    }
  }
}

/*
 * Method to start a process worker of a brook-ws
 */
int brook_process(void) {
  process.pid = getpid();
  brook_log(&logger, L_INFO, "Process will execute the initial setup\n");

  brook_process_register_listener();
  brook_debug("listening on fd=%d pid=%d\n", process.socket_fd, process.pid);

  if (brook_pool_init(&process.pool) == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  brook_process_loop();

  return BROOK_OK;
}
