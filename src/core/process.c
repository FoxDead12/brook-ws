#include "core/process.h"
#include "core/connection.h"
#include "core/logger.h"
#include "core/poll.h"
#include "core/socket.h"
#include "core/types.h"
#include <stdio.h>

// handler to server file descritpor the socket will accept conns
static poll_handlers_t poll_handler = (poll_handlers_t){
    .on_read = connection_accept,
};

/*
 * The processor is the main of each process/worker
 * here we join and merge all layers will exist and set the necessary
 * configurations to code
 */
int process_start(void) {

  // create the main socket will accept the TCP connections
  int socket = socket_bind(3001);

  // for some reason OS don't allow open/create the socket
  if (socket == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  // start the events loop, the worker is now block only working
  poll_start();

  return BROOK_OK;
}
