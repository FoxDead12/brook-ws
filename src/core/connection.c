#include "core/connection.h"
#include "core/buffer.h"
#include "core/config.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/poll.h"
#include "core/pool.h"
#include "core/socket.h"
#include "core/types.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

static brook_protocol_t *default_protocol = NULL;

/*
 * Method used when server receive a new TCP connection
 */
void brook_connection_accept(void *ctx) {
  (void)ctx;

  int fd;
  struct sockaddr_in address;
  socklen_t len = sizeof(address);

  int idx = brook_poll_next_slot();
  if (idx == -1) {
    brook_debug("accept skipped, poll is full\n");
    // TODO: i can disable the event of read of server socket to disable events of accept event
    return;
  }

  fd = brook_socket_accept(process.socket_fd, (struct sockaddr *)&address, &len);
  if (fd == BROOK_DONE) {
    brook_debug("accept returned nothing (another worker took it or error)\n");
    return;
  }

  // Fill the connection first, so it is complete when the fd enters the poll
  brook_connection_t *con = &process.conns[idx];
  *con = (brook_connection_t){
      .fd = fd,
      .slot = idx,
      .port = ntohs(address.sin_port),
      .buffers = NULL,
      .protocol_ctx = NULL,
      .protocol = default_protocol,
  };
  inet_ntop(AF_INET, &address.sin_addr, con->ip, INET_ADDRSTRLEN);

  // Memory where the protocols write their own state
  if (brook_buffer_get_free(&con->protocol_ctx) == NULL) {
    brook_log(&logger, L_ERROR, "Can't allocate protocol context for fd=%d\n", fd);
    memset(con, 0, sizeof(brook_connection_t));
    close(fd);
    return;
  }

  if (brook_poll_add(fd, POLLIN, (brook_handler_t){.on_read = brook_connection_read, .on_write = NULL, .on_error = brook_connection_destroy, .ctx = con}) == BROOK_ERROR) {
    brook_buffer_destroy(con->protocol_ctx);
    memset(con, 0, sizeof(brook_connection_t));
    close(fd);
    return;
  }

  brook_debug("new connection fd=%d slot=%d from %s:%d\n", fd, idx, con->ip, con->port);

  // Last: the protocol may destroy the connection, so con can't be used after this
  if (con->protocol != NULL && con->protocol->on_open != NULL) {
    con->protocol->on_open(con);
  }

  return;
}

/*
 * Method used to destroy/remove a connection
 */
void brook_connection_destroy(void *ctx) {

  brook_connection_t *con = (brook_connection_t *)ctx;

  // After the slots are rearranged con may point to another connection
  int slot = con->slot;
  int fd = con->fd;

  // 1. Protocol cleans its state while protocol_ctx is still valid
  if (con->protocol != NULL && con->protocol->on_close != NULL) {
    con->protocol->on_close(con);
  }

  // 2. Release the resources of the connection
  brook_buffer_destroy(con->buffers);
  brook_buffer_destroy(con->protocol_ctx);
  close(fd);

  // 3. Remove from poll and keep conns[] aligned with it
  int last = brook_poll_remove(slot);
  if (last != slot) {
    process.conns[slot] = process.conns[last];
    process.conns[slot].slot = slot;
    brook_poll_set_ctx(slot, &process.conns[slot]);
    brook_debug("connection destroy fd=%d slot=%d, moved last connection fd=%d from slot=%d to slot=%d\n", fd, slot, process.conns[slot].fd, last, slot);
  } else {
    brook_debug("connection destroy fd=%d slot=%d was the last, no move\n", fd, slot);
  }

  memset(&process.conns[last], 0, sizeof(brook_connection_t));
}

/*
 * Method when a socket/connection send data and we need read
 */
void brook_connection_read(void *ctx) {

  brook_connection_t *con = (brook_connection_t *)ctx;

  // Get a buffer to store read data
  brook_buffer_t *buffer = brook_buffer_get_free(&con->buffers);
  if (buffer == NULL) {
    return;
  }

  // Will set the params to read function
  unsigned char *start = buffer->data + buffer->len;
  ssize_t b = read(con->fd, start, buffer->free);
  if (b == -1) {
    brook_log(&logger, L_ERROR, "Can't read from connection fd=%d: %s\n", con->fd, strerror(errno));
    brook_connection_destroy(ctx);
    return;
  } else if (b == 0) {
    brook_connection_destroy(ctx);
    return;
  }

  buffer->free -= b;
  buffer->len += b;
  printf("buffer: %s\n", buffer->data);

  assert(con->protocol != NULL && con->protocol->on_data != NULL);
  con->protocol->on_data(con);

  return;
}

/*
 * Only for the start of worker
 */
int brook_connection_set_default_protocol(brook_protocol_t *protocol) {
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

  if (con->protocol != NULL && con->protocol->on_close != NULL) {
    con->protocol->on_close(con);
  }

  con->protocol = protocol;

  if (con->protocol != NULL && con->protocol->on_open != NULL) {
    con->protocol->on_open(con);
  }

  return BROOK_OK;
}
