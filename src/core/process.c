#include "core/process.h"
#include "beanstalkclient.h"
#include "core/connection.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/poll.h"
#include "core/pool.h"
#include "core/types.h"
#include "http/middleware.h"
#include "http/request.h"
#include "job/queue.h"
#include <sys/poll.h>
#include <unistd.h>

// Protocol used by every new connection, the HTTP can upgrade it later
static brook_protocol_t http_protocol = {
    .on_open = brook_http_open,
    .on_data = brook_http_on_data,
    .on_close = brook_http_on_close,
    .on_drain = brook_http_on_drain,
};

static brook_middleware_t http_middleware = {
    .on_call = brook_http_middleware_on_call,
    .on_finish = brook_http_middleware_on_finish,
};

/*
 * Method to start a process worker of a brook-ws
 */
int brook_process(void) {

  brook_log(&logger, L_INFO, "Process will execute the initial setup\n");

  process.pid = getpid();

  brook_connection_set_default_protocol(&http_protocol);
  brook_http_set_default_middleware(&http_middleware);

  int rc = brook_job_queue_create("", "");

  if (rc == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  // Memory used by the buffers of the connections
  rc = brook_pool_init_head(&process.pool_head);

  if (rc == BROOK_ERROR) {
    brook_log(&logger, L_ERROR, "Can't initialize the pool of buffers\n");
    return BROOK_ERROR;
  }

  // The listener goes last, only now the process is ready to accept connections
  rc = brook_poll_add(
      process.socket_fd,
      POLLIN,
      (brook_handler_t){
          .on_read = brook_connection_accept,
          .on_write = NULL,
          .on_error = NULL,
          .ctx = NULL,
      });

  if (rc == BROOK_ERROR) {
    brook_log(&logger, L_ERROR, "Can't add the listening socket to the poll\n");
    return BROOK_ERROR;
  }

  rc = brook_poll_add(
      process.jobs.fd,
      POLLIN,
      (brook_handler_t){
          .on_read = brook_job_queue_on_read,
          .on_write = brook_job_queue_on_write,
          .on_error = NULL,
          .ctx = process.jobs.client,
      });

  if (rc == BROOK_ERROR) {
    brook_log(&logger, L_ERROR, "Can't add the listening socket jobs queue to the poll\n");
    return BROOK_ERROR;
  }

  brook_debug("listening on fd=%d pid=%d\n", process.socket_fd, process.pid);
  brook_debug("listening on fd=%d pid=%d\n", process.jobs.fd, process.pid);

  brook_poll_loop();

  return BROOK_OK;
}
