#include "core/connection.h"
#include "core/buffer.h"
#include "core/config.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/pool.h"
#include "core/process.h"
#include "core/socket.h"
#include "core/types.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

static const brook_protocol_t *default_protocol = NULL;

/*
 * Only for the start of worker
 */
int brook_connection_set_default_protocol(const brook_protocol_t *protocol) {
  default_protocol = protocol;
  return BROOK_OK;
}

/*
 * Method to change the set protocol of connection
 */
int brook_connection_set_protocol(brook_connection_t *con, brook_protocol_t *protocol) {
  if (protocol == NULL) {
    return BROOK_ERROR;
  }

  // Let the old protocol release its state (also clears proto_ctx)
  if (con->protocol != NULL && con->protocol->on_close != NULL) {
    con->protocol->on_close(con);
  }

  con->protocol = protocol;
  return BROOK_OK;
}

/*
 * Method used when server receive a new TCP connection
 */
void brook_connection_accept(void *ctx) {
  (void)ctx;

  int fd;
  struct sockaddr_in address;
  socklen_t len = sizeof(address);

  // Protect a process accept more connections than can handle
  if (process.nfds >= POLL_FDS_MAX) {
    // TODO: i can disable the event of read of server socket to disable events of accept event
    brook_debug("accept skipped, process is full nfds=%d max=%d\n", process.nfds, POLL_FDS_MAX);
    return;
  }

  // Accept connection
  fd = brook_socket_accept(process.socket_fd, (struct sockaddr *)&address, &len);
  if (fd == BROOK_DONE) {
    brook_debug("accept returned nothing (another worker took it or error)\n");
    return;
  }

  // Add the new connection in process data
  int idx = process.nfds; // make this because is a counter but arrays are lower starting from 0
  process.fds[idx] = (struct pollfd){.fd = fd, .events = POLLIN, .revents = 0};
  process.handlers[idx] = (brook_handler_t){.on_read = brook_connection_read, .on_write = NULL, .on_error = brook_connection_destroy, .ctx = &process.conns[idx]};
  process.conns[idx] = (brook_connection_t){
      .fd = fd,
      .fd_idx = idx,
      .port = ntohs(address.sin_port),
      .buffers = NULL,
      .protocol = default_protocol};
  inet_ntop(AF_INET, &address.sin_addr, process.conns[idx].ip, INET_ADDRSTRLEN);
  process.nfds++;

  brook_debug("new connection fd=%d slot=%d from %s:%d nfds=%d\n", fd, idx, process.conns[idx].ip, process.conns[idx].port, process.nfds);
  return;
}

void brook_connection_destroy(void *ctx) {

  brook_connection_t *con = (brook_connection_t *)ctx;

  // Free list of buffers in connection
  brook_buffer_destroy(con->buffers);

  // Close socket
  close(con->fd);

  int idx = con->fd_idx;
  int idx_last = process.nfds - 1;
  int socket = con->fd;

  // Now need change the position in arrays if this connection wasant the last
  if (idx != idx_last) {
    // Need change the last connection to this point
    process.fds[idx] = process.fds[idx_last];
    process.handlers[idx] = process.handlers[idx_last];
    process.conns[idx] = process.conns[idx_last];
    brook_debug("connection destroy fd=%d slot=%d, moved last connection fd=%d from slot=%d to slot=%d\n", socket, idx, process.fds[idx].fd, idx_last, idx);
  } else {
    brook_debug("connection destroy fd=%d slot=%d was the last, no move\n", con->fd, idx);
  }

  memset(&process.fds[idx_last], 0, sizeof(struct pollfd));
  memset(&process.handlers[idx_last], 0, sizeof(brook_handler_t));
  memset(&process.conns[idx_last], 0, sizeof(brook_connection_t));

  process.nfds--;
}

/*
 * Method when a socket/connection send data and we need read
 */
void brook_connection_read(void *ctx) {

  brook_connection_t *con = (brook_connection_t *)ctx;

  // Get a buffer to store read data
  brook_buffer_t *buffer = brook_buffer_get_free(&con->buffers);

  // Will set the params to read function
  unsigned char *start = buffer->data + buffer->len;

  ssize_t b = read(con->fd, start, buffer->free);
  if (b == -1) {
    brook_log(&logger, L_ERROR, "Can't read from connection fd=%d: %s\n", con->fd, strerror(errno));
    brook_connection_destroy(ctx);
  } else if (b == 0) {
    brook_connection_destroy(ctx);
    return;
  }

  return;
}
