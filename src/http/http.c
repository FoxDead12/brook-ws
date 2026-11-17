#include "http/http.h"
#include "core/buffer.h"
#include "core/connection.h"
#include "core/pool.h"
#include "core/types.h"
#include "http/parser.h"
#include <stdio.h>
#include <string.h>

/*
 * Here is Layer 3 the protocol layer. here he will implemente the logic of the http protocol
 *
 * */
static pool_t _http_pool;

/*
 * Entry point run when the connection is accept
 */
int http_on_open(connection_t *con) {

  // get from the pool a free http to handle the protocol logic
  con->protocol_ctx = pool_acquire(&_http_pool);

  http_t *http = con->protocol_ctx;

  // the struct need to now how is the connection is associated
  http->con = con;

  // reset the request memory
  http_parser_reset(http);

  return BROOK_OK;
}

/*
 * The connection TCP just read data
 */
int http_on_data(connection_t *con) {

  // convert the pointer intrepetation only
  http_t *http = con->protocol_ctx;

  // parser will handle the work
  int rc = http_parser_on_data(http);

  if (rc == BROOK_DONE) {
    // request was parsed

  } else if (rc != BROOK_OK) {
    // TODO: will fire the error
  }

  return BROOK_OK;
}

int http_on_write(connection_t *con) {

  return BROOK_OK;
}

/*
 * When the connection close
 */
int http_on_close(connection_t *con) {

  // give back to pool the memory used in protocol for conection
  pool_release(&_http_pool, con->protocol_ctx);

  return BROOK_OK;
}

int http_setup(void) {

  // create the default http memory allocation is equal to cons
  pool_alloc(&_http_pool, sizeof(http_t), MAX_CONNS);

  return BROOK_OK;
}
