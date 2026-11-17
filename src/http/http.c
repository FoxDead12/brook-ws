#include "http/http.h"

#include "core/types.h"

/*
 * Here is Layer 3 the protocol layer. here he will implemente the logic of the http protocol
 *
 * */

int http_on_open(connection_t *con) {

  return BROOK_OK;
}

int http_on_data(connection_t *con) {

  return BROOK_OK;
}

int http_on_write(connection_t *con) {

  return BROOK_OK;
}

int http_on_close(connection_t *con) {

  return BROOK_OK;
}
