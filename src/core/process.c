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
 * Method to start a process worker of a brook-ws
 */
int brook_process(void) {

  // Store the pid of process
  process.pid = getpid();
  brook_log(&logger, L_INFO, "Process will execute the initial setup\n");

  process.fds[0].fd = process.socket_fd;
  process.fds[0].events = POLLIN;
  process.fds[0].revents = 0;
  process.handlers[0] = (brook_handler_t){.on_read = brook_connection_accept, .on_write = NULL, .on_error = NULL, .ctx = NULL};
  process.nfds = 1;
  brook_debug("listening on fd=%d pid=%d\n", process.socket_fd, process.pid);

  brook_pool_init(&process.pool);

  // Start the event loop of process
  while (1) {

    // will wait for a file descritpor contain a event
    int nready = poll(process.fds, process.nfds, -1);
    if (nready == -1) {
      brook_log(&logger, L_ERROR, "Can't execute poll: %s\n", strerror(errno));
      break;
    }
    brook_debug("poll returned nready=%d nfds=%d\n", nready, process.nfds);

    // will iterate all fds array to see if contain a event
    for (int i = 0; i < process.nfds && nready > 0; i++) {
      struct pollfd *fd = &process.fds[i];
      brook_handler_t *handler = &process.handlers[i];

      // Ignore empty events the bit mask is 0
      short ev = fd->revents;
      if (ev == 0) {
        continue;
      }
      brook_debug("event slot=%d fd=%d revents=0x%x\n", i, fd->fd, ev);

      // if ((ev & (POLLERR | POLLHUP | POLLNVAL)) && h->on_error) {
      //   h->on_error(h->ctx);
      //   continue;
      // }

      // Read event
      if (ev & POLLIN && handler->on_read) {
        handler->on_read(handler->ctx);
      }

      // Write event
      if (ev & POLLIN && handler->on_write) {
        handler->on_write(handler->ctx);
      }

      nready--;
    }
  }

  return BROOK_OK;
}
