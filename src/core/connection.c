#include "core/connection.h"
#include "core/logger.h"
#include "core/poll.h"
#include "core/types.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Here is the Layer 2. Here we are in connection level, the con will controll is state and
 * if necessary the poll slot of con.
 */

static connection_manager_t _connections;

// the necessary of listner when socket receive connections
static listner_t _listener;
static poll_handlers_t _listner_handler = {
    .on_read = connection_accept,
};

/*
 * On each new connection enter here
 */
int connection_accept(void *ctx) {
  (void)ctx;

  // firts get a slot to a new connection
  connection_t *con = _connection_acquire();

  // the connections are full ignore event another process catch
  if (con == NULL) {
    logger_debug("connection_accept: no free slot (max=%d), ignoring event", MAX_CONNS);
    _lister_stop_accept();
    return BROOK_DONE;
  }

  // accept a new connection for this process manager
  struct sockaddr_in address;
  socklen_t len = sizeof(address);
  int fd = accept(_listener.fd, (struct sockaddr *)&address, &len);

  // check if realy the process the something
  if (fd == -1) {
    logger_debug("connection_accept: accept() failed (errno=%d: %s)", errno, strerror(errno));
    _connection_release(con);
    return BROOK_DONE;
  }

  // set socket was non blocking
  if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
    logger_debug("connection_accept: fcntl(O_NONBLOCK) failed fd=%d (errno=%d: %s)", fd, errno, strerror(errno));
    close(fd);
    _connection_release(con);
    return BROOK_DONE;
  }

  // add file descritor to event poll
  int rc = poll_add(fd, (poll_handlers_t){
                            .ctx = con,
                            .on_read = connection_on_read,
                            .on_write = connection_on_write,
                            .on_close = connection_on_close,
                        });

  // the poll don't accept the new fd
  if (rc == BROOK_ERROR) {
    logger_debug("connection_accept: poll rejected fd=%d, closing", fd);
    close(fd);
    _connection_release(con);
    return BROOK_DONE;
  }

  // create the connection
  *con = (connection_t){
      .fd = fd,
      .port = ntohs(address.sin_port),
  };
  inet_ntop(AF_INET, &address.sin_addr, con->ip, INET_ADDRSTRLEN);

  logger_debug("connection_accept: new connection fd=%d from %s:%d (free=%zu)", fd, con->ip, con->port, _connections.free_count);

  return BROOK_OK;
}

/*
 * The down layer notify we have data to read
 */
int connection_on_read(void *ctx) {
  connection_t *con = (connection_t *)ctx;
  logger_debug("connection_on_read: fd=%d %s:%d", con->fd, con->ip, con->port);

  return BROOK_OK;
}

/*
 * The down layer notify event to write
 */
int connection_on_write(void *ctx) {
  connection_t *con = (connection_t *)ctx;
  logger_debug("connection_on_write: fd=%d %s:%d", con->fd, con->ip, con->port);

  return BROOK_OK;
}

/*
 * The down event indicate the fd close
 */
int connection_on_close(void *ctx) {
  connection_t *con = (connection_t *)ctx;

  logger_debug("connection_on_close: closing fd=%d %s:%d", con->fd, con->ip, con->port);

  // close socket
  close(con->fd);

  // remove the fd from poll event
  poll_remove(con->fd);

  // free the connection struct give to list
  _connection_release(con);

  // make the polling start again if was stoped
  _listner_start_accept();

  return BROOK_OK;
}

/*
 * the process wil call this method is the initial setup for connection layer
 * will define the defaults and start the necessary memory
 */
int connection_setup(int fd) {

  assert(fd >= 0);

  logger_debug("connection_setup: listener fd=%d, max connections=%d", fd, MAX_CONNS);

  // setup the connections manager memory
  {
    memset(&_connections, 0, sizeof(connection_manager_t));
    for (int i = 0; i < MAX_CONNS; i++) {
      _connections.free[i] = &_connections.connections[i];
      _connections.free_count++;
    }
  }

  // setup the listner
  {
    _listener.fd = fd;
    _listener.accepting = 1;

    // add the listner to event poll
    int rc = poll_add(fd, _listner_handler);

    // check if event poll accept
    if (rc == BROOK_ERROR) {
      logger_debug("connection_setup: poll_add failed for listener fd=%d", fd);
      return BROOK_ERROR;
    }

    logger_debug("connection_setup: listener registered at fd=%d", fd);
  }

  return BROOK_OK;
}

/*
 * We get a free slot to get a connection struct to work
 * and make the necessary changes
 */
connection_t *_connection_acquire(void) {
  // check if we have a free spaces to add a con
  if (_connections.free_count == 0) {
    return NULL;
  }
  connection_t *con = _connections.free[--_connections.free_count];
  logger_debug("_connection_acquire: slot acquired (free=%zu)", _connections.free_count);
  return con;
}

/*
 * We return a used con to the free queue/list
 * to be possible be reused
 */
void _connection_release(connection_t *con) {
  assert(con != NULL);
  assert(_connections.free_count < MAX_CONNS);

  // we reset the struct to don't pass data to next
  memset(con, 0, sizeof(connection_t));
  _connections.free[_connections.free_count++] = con;

  logger_debug("_connection_release: slot released (free=%zu)", _connections.free_count);
}

/*
 * Listner stop receive events of accept
 */
void _lister_stop_accept(void) {
  if (_listener.accepting == 0)
    return;

  _listener.accepting = 0;
  poll_remove_events(_listener.fd, POLLIN);
}

/*
 * Listner start receive events of accept
 */
void _listner_start_accept(void) {
  if (_listener.accepting == 1)
    return;

  _listener.accepting = 1;
  poll_add_events(_listener.fd, POLLIN);
}
