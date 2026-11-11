#include "core/poll.h"
#include "core/logger.h"
#include "core/types.h"
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

/*
 * Here is the Layer 1. The polling system who will fire the call to a unknow Layer/logic/entity
 * this will controll the abuve layers if will recevie work or not this can stop and run the server basicly
 */

/*
 * Only this layer will use this static variables
 */
static int _fds_count = 0;
static struct pollfd _fds[POLL_MAX_FD] = {0};
static poll_handlers_t _handlers[POLL_MAX_FD] = {0};
static int _fds_table[POLL_MAX_FD] = {0};

/*
 * Initial setup of the layer, reset the table of fd -> slot
 * must be called before any poll_add and is the process will execute
 */
void poll_setup(void) {
  {
    for (int i = 0; i < POLL_MAX_FD; i++) {
      _fds_table[i] = -1;
    }
  }
}

/*
 * Start the logic off the layer start listening the OS evetns
 */
int poll_start(void) {

  logger_debug("poll_start: loop started (fds=%d)", _fds_count);

  while (1) {

    // will wait for ever for a event
    int n_ready = poll(_fds, _fds_count, -1);
    if (n_ready == -1) {
      logger_debug("poll_start: poll() returned -1 (errno=%d: %s)", errno, strerror(errno));
      continue;
    }

    logger_debug("poll_start: %d fd(s) ready of %d", n_ready, _fds_count);

    // iterate each index of fds to check the events
    // we iterate from the last index to first because when remove fds dont contain any problem
    for (int i = _fds_count - 1; i >= 0 && n_ready > 0; i--) {
      // ignore the socket has empty events, this probably will not append but is a safe
      if (_fds[i].revents == 0)
        continue;

      // will fire the necessary callbacks to event type
      _poll_dispatch(i);
      n_ready--;
    }
  }

  return BROOK_OK;
}

void _poll_dispatch(int idx) {

  // copy the values is not a problem
  short revents = _fds[idx].revents;

  // get reference of handler
  poll_handlers_t *handler = &_handlers[idx];

  // keep the fd, the slot can change when a fd is removed
  int fd = _fds[idx].fd;

  // result of handlers
  int rc = BROOK_OK;

  logger_debug("poll_dispatch: idx=%d fd=%d revents=0x%x", idx, _fds[idx].fd, revents);

  // the fd has data to read
  if (revents & POLLIN && rc == BROOK_OK) {
    logger_debug("poll_dispatch: idx=%d fd=%d POLLIN -> on_read", idx, _fds[idx].fd);
    assert(handler->on_read);
    rc = handler->on_read(handler->ctx);
  }

  // the fd is available to send/write data
  if (revents & POLLOUT && rc == BROOK_OK) {
    logger_debug("poll_dispatch: idx=%d fd=%d POLLOUT -> on_write", idx, _fds[idx].fd);
    assert(handler->on_write);
    rc = handler->on_write(handler->ctx);
  }

  // this fd has a problema need be closed
  if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
    logger_debug("poll_dispatch: idx=%d fd=%d error/hangup -> on_close", idx, _fds[idx].fd);
    rc = BROOK_CLOSE;
  }

  // the handlers request to close the connection
  if (rc == BROOK_CLOSE) {
    logger_debug("poll_dispatch: idx=%d fd=%d BROOK_CLOSE -> on_close", idx, _fds[idx].fd);
    assert(handler->on_close);
    handler->on_close(handler->ctx);
  }

  return;
}

/*
 * Add a new file descriptor to the poll
 * and will start with event of read
 */
int poll_add(int fd, poll_handlers_t handler) {

  // the index will be added the new file descritpor
  int idx = _fds_count;

  // check if poll contain space to add a fd and the fd fits in the table
  if (idx >= POLL_MAX_FD || fd < 0 || fd >= POLL_MAX_FD) {
    logger_debug("poll_add: full (max=%d), rejecting fd=%d", POLL_MAX_FD, fd);
    return BROOK_ERROR;
  }

  // add new file descritpor to array of poll
  _fds[idx] = (struct pollfd){
      .fd = fd,
      .events = POLLIN,
      .revents = 0,
  };

  // copy the receive handler to the handlers array
  _handlers[idx] = handler;

  // remember the slot of this fd
  _fds_table[fd] = idx;

  // increment the count
  _fds_count++;

  logger_debug("poll_add: fd=%d added at slot=%d (count=%d)", fd, idx, _fds_count);

  return BROOK_OK;
}

/*
 * Will remove a file descriptor from array
 */
void poll_remove(int fd) {

  // get the slot of this fd
  int slot = _fds_table[fd];
  assert(slot != -1 && _fds[slot].fd == fd);

  // get the last index of array
  int idx_last = _fds_count - 1;

  logger_debug("poll_remove: slot=%d fd=%d (last=%d, count=%d)", slot, fd, idx_last, _fds_count);

  // check if we are removing last slot or not
  if (slot != idx_last) {
    _fds[slot] = _fds[idx_last];
    _handlers[slot] = _handlers[idx_last];

    // the fd moved has a new slot
    _fds_table[_fds[slot].fd] = slot;
  }

  // the fd is no more in the table
  _fds_table[fd] = -1;

  _fds[idx_last].fd = -1;
  _fds[idx_last].events = 0;
  _fds[idx_last].revents = 0;
  memset(&_handlers[idx_last], 0, sizeof(poll_handlers_t));

  _fds_count--;

  return;
}

/*
 *  Will update a specific file descritpor event
 *  we merge the new events to current events
 */
void poll_add_events(int fd, short events) {
  int slot = _fds_table[fd];
  assert(slot != -1 && _fds[slot].fd == fd);
  _fds[slot].events |= events;
  logger_debug("poll_add_events: slot=%d fd=%d +0x%x -> events=0x%x", slot, fd, events, _fds[slot].events);
}

/*
 *  Will update a specific file descritpor event
 *  we invert the bit mask receive to remove using the & (AND) operator
 * */
void poll_remove_events(int fd, short events) {
  int slot = _fds_table[fd];
  assert(slot != -1 && _fds[slot].fd == fd);
  _fds[slot].events &= ~events;
  logger_debug("poll_remove_events: slot=%d fd=%d -0x%x -> events=0x%x", slot, fd, events, _fds[slot].events);
}
