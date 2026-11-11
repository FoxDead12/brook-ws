#include "core/connection.h"
#include "core/poll.h"
#include "core/types.h"
#include <string.h>
#include <unistd.h>

/*
 * Here is the Layer 2. Here we are in connection level, the con will controll is state and
 * if necessary the poll slot of con.
 */

static connection_manager_t _connections;

/*
 * the process wil call this method is the initial setup for connection layer
 * will define the defaults and start the necessary memory
 */
int connection_setup() {

  // setup the connections manager memory
  {
    memset(&_connections, 0, sizeof(connection_manager_t));
    for (int i = 0; i < MAX_CONNS; i++) {
      memset(&_connections.connections[i], 0, sizeof(connection_t));
      _connections.free[i] = &_connections.connections[i];
      _connections.free_count++;
    }
  }

  return BROOK_OK;
}

/*
 * On each new connection enter here
 */
int connection_accept(void *ctx) {
  (void)ctx;

  return BROOK_OK;
}

/*
 * The down layer notify we have data to read
 */
int connection_on_read(void *ctx) {

  return BROOK_OK;
}

/*
 * The down layer notify event to write
 */
int connection_on_write(void *ctx) {

  return BROOK_OK;
}

/*
 * The down event indicate the fd close
 */
int connection_on_close(void *ctx) {
  connection_t *con = (connection_t *)ctx;

  // close socket
  close(con->fd);

  return BROOK_OK;
}
