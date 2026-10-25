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

  if (brook_poll_add(fd, POLLIN, (brook_handler_t){.on_read = brook_connection_read, .on_write = NULL, .on_error = brook_connection_destroy, .ctx = con}) == BROOK_ERROR) {
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

  // Protocol cleans its state while protocol_ctx is still valid
  if (con->protocol != NULL && con->protocol->on_close != NULL) {
    con->protocol->on_close(con);
  }

  // Release the resources of the connection
  brook_buffer_destroy(con->buffers);
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

  // Get a buffer with free space to store the data
  brook_buffer_t *buffer = brook_buffer_get_free(&con->buffers);
  if (buffer == NULL) {
    // Without this the POLLIN stays pending and poll returns again and again
    brook_log(&logger, L_ERROR, "Can't get a buffer for connection fd=%d\n", con->fd);
    brook_connection_destroy(con);
    return;
  }

  // Read after the data already stored in the buffer
  unsigned char *start = buffer->data + buffer->len;
  ssize_t bytes = read(con->fd, start, buffer->free);
  if (bytes == -1) {
    brook_log(&logger, L_ERROR, "Can't read from connection fd=%d: %s\n", con->fd, strerror(errno));
    brook_connection_destroy(con);
    return;
  }

  // Peer closed the connection
  if (bytes == 0) {
    brook_connection_destroy(con);
    return;
  }

  buffer->len += bytes;
  buffer->free -= bytes;
  brook_debug("read fd=%d bytes=%zd data=%.*s\n", con->fd, bytes, (int)bytes, start);

  // Hand the data to the protocol, con can't be used after this because it may destroy the connection
  assert(con->protocol != NULL && con->protocol->on_data != NULL);
  con->protocol->on_data(con, buffer, start, bytes);
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
