#include "core/process.h"
#include "core/connection.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/poll.h"
#include "core/pool.h"
#include "core/types.h"
#include "http/request.h"
#include <sys/poll.h>
#include <unistd.h>

// Protocol used by every new connection, the HTTP can upgrade it later
static brook_protocol_t http_protocol = {
    .on_open = brook_http_open,
    .on_data = brook_http_on_data,
    .on_close = brook_http_on_close,
};

/*
 * Method to start a process worker of a brook-ws
 */
int brook_process(void) {

  brook_log(&logger, L_INFO, "Process will execute the initial setup\n");

  process.pid = getpid();

  // Memory used by the buffers of the connections
  if (brook_pool_init_head(&process.pool_head) == BROOK_ERROR) {
    brook_log(&logger, L_ERROR, "Can't initialize the pool of buffers\n");
    return BROOK_ERROR;
  }

  brook_connection_set_default_protocol(&http_protocol);

  // The listener goes last, only now the process is ready to accept connections
  if (brook_poll_add(process.socket_fd, POLLIN, (brook_handler_t){.on_read = brook_connection_accept, .on_write = NULL, .on_error = NULL, .ctx = NULL}) == BROOK_ERROR) {
    brook_log(&logger, L_ERROR, "Can't add the listening socket to the poll\n");
    return BROOK_ERROR;
  }
  brook_debug("listening on fd=%d pid=%d\n", process.socket_fd, process.pid);

  brook_poll_loop();

  return BROOK_OK;
}
