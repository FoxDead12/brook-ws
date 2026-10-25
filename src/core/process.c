#include "core/process.h"
#include "core/config.h"
#include "core/connection.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/poll.h"
#include "core/pool.h"
#include "core/types.h"
#include "http/request.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

/*
 * Method to start a process worker of a brook-ws
 */
int brook_process(void) {

  brook_log(&logger, L_INFO, "Process will execute the initial setup\n");

  process.pid = getpid();

  brook_poll_add(process.socket_fd, POLLIN, (brook_handler_t){.on_read = brook_connection_accept, .on_write = NULL, .on_error = NULL, .ctx = NULL});
  brook_debug("listening on fd=%d pid=%d\n", process.socket_fd, process.pid);

  // Set the default protocol for new connections
  static brook_protocol_t http_protocol = {.on_open = brook_http_open, .on_data = brook_http_on_data, .on_close = brook_http_on_close};
  brook_connection_set_default_protocol(&http_protocol);

  if (brook_pool_init_head(&process.pool_head) == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  brook_poll_loop();

  return BROOK_OK;
}
