
#include "core/poll.h"
#include "core/globals.h"
#include "core/process.h"
#include "core/types.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

/*
 * Method to start the event poll of proccer/worker
 */
void brook_poll_loop(void) {
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

      brook_poll_dispatch(i);
      nready--;
    }
  }
}

/*
 * Event for each file descriptor register in loop
 */
void brook_poll_dispatch(int slot) {
  short events = process.fds[slot].revents;
  brook_handler_t *handler = &process.handlers[slot];

  brook_debug("event slot=%d fd=%d revents=0x%x\n", slot, process.fds[slot].fd, events);

  if ((events & (POLLERR | POLLHUP | POLLNVAL)) && handler->on_error) {
    handler->on_error(handler->ctx);
    return;
  }

  if ((events & POLLOUT) && handler->on_write) {
    handler->on_write(handler->ctx);
    return;
  }

  if ((events & POLLIN) && handler->on_read) {
    handler->on_read(handler->ctx);
    return;
  }
}

/*
 * Method used to get a new slot to file descritpor
 */
int brook_poll_next_slot(void) {
  if (process.nfds < POLL_FDS_MAX) {
    return process.nfds;
  } else {
    return BROOK_ERROR;
  }
}

/*
 * Method to add a new file descriptor to event loop
 */
int brook_poll_add(int fd, short events, brook_handler_t handler) {
  int idx = brook_poll_next_slot();
  if (idx == -1) {
    return BROOK_ERROR;
  }
  process.fds[idx] = (struct pollfd){.fd = fd, .events = events, .revents = 0};
  process.handlers[idx] = handler;
  process.nfds++;
  brook_debug("poll add fd=%d slot=%d nfds=%d\n", fd, idx, process.nfds);
  return idx;
}

/*
 * Method to remove a file descriptor from event loop
 */
int brook_poll_remove(int slot) {
  int last = process.nfds - 1;
  int moved = slot;

  if (slot != last) {
    process.fds[slot] = process.fds[last];
    process.handlers[slot] = process.handlers[last];
    moved = last;
  }

  memset(&process.fds[last], 0, sizeof(struct pollfd));
  memset(&process.handlers[last], 0, sizeof(brook_handler_t));
  process.nfds--;

  return moved;
}

/*
 * Allow change a ctx from a existent handler
 */
void brook_poll_set_ctx(int slot, void *ctx) {
  process.handlers[slot].ctx = ctx;
}
