#include "core/process.h"
#include "core/connection.h"
#include "core/poll.h"
#include "core/socket.h"
#include "core/types.h"
#include <stdio.h>

static protocol_t http = {
    .on_data = NULL,
    .on_write = NULL,
    .on_close = NULL,
};

/*
 * The processor is the main of each process/worker
 * here we join and merge all layers will exist and set the necessary
 * configurations to code
 */
int process_start(void) {

  // setup the layer of poll, must be before the others add fds
  poll_setup();

  // setup the layer of connection
  connection_setup(socket_bind(3001), &http);

  // start the events loop, the worker is now block only working
  poll_start();

  return BROOK_OK;
}
