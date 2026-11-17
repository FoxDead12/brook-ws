#include "core/process.h"
#include "core/connection.h"
#include "core/poll.h"
#include "core/socket.h"
#include "core/types.h"
#include "http/http.h"
#include <stdio.h>

static protocol_t http = {
    .on_open = http_on_open,
    .on_data = http_on_data,
    .on_write = http_on_write,
    .on_close = http_on_close,
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

  // setup the protocols
  http_setup();

  // start the events loop, the worker is now block only working
  poll_start();

  return BROOK_OK;
}
