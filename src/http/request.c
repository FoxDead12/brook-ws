#include "http/request.h"
#include "core/buffer.h"
#include "core/connection.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/pool.h"
#include "core/str.h"
#include "core/types.h"
#include "http/parser.h"
#include "http/response.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>
#include <sys/stat.h>

// Status and message sent to the client for each parser error
const brook_http_error_t brook_http_errors[BROOK_HTTP_ERR_COUNT] = {
    [BROOK_HTTP_ERR_BAD_REQUEST] = {400, "Malformed HTTP request"},
    [BROOK_HTTP_ERR_BAD_METHOD] = {501, "Method not implemented"},
    [BROOK_HTTP_ERR_BAD_URL] = {400, "Invalid URL"},
    [BROOK_HTTP_ERR_URL_TOO_LONG] = {414, "URL too long"},
    [BROOK_HTTP_ERR_BAD_PARAMS] = {400, "Invalid query string"},
    [BROOK_HTTP_ERR_BAD_VERSION] = {400, "Invalid HTTP version"},
    [BROOK_HTTP_ERR_BAD_LINE_END] = {400, "Invalid line ending"},
    [BROOK_HTTP_ERR_BAD_HEADER] = {400, "Invalid header"},
    [BROOK_HTTP_ERR_BAD_CONTENT_LEN] = {400, "Invalid Content-Length"},
    [BROOK_HTTP_ERR_HEADER_TOO_BIG] = {431, "Header too big"},
};

/*
 * Method to initiate this http protocol in connection
 */
void brook_http_open(brook_connection_t *con) {

  // Allocate a buffer to context of protocol
  brook_buffer_t *memory = brook_buffer_get_free(&con->protocol_ctx);
  if (memory == NULL) {
    brook_log(&logger, L_ERROR, "Can't allocate http context for fd=%d\n", con->fd);
    brook_connection_destroy(con);
    return;
  }

  // We need contain this check before compile
  _Static_assert(sizeof(brook_http_request_t) <= BROOK_POOL_BLOCK_SIZE, "request does not fit in a pool block");

  // Will start from zero the http information
  brook_http_reset(con);
}

/*
 * Method called when connection change protocol or is closed
 */
void brook_http_on_close(brook_connection_t *con) {
  // brook_buffer_t *memory = con->protocol_ctx;
  brook_buffer_destroy(con->protocol_ctx);
  con->protocol_ctx = NULL;
}

/*
 * Method called each time connection read data
 */
void brook_http_on_data(brook_connection_t *con, brook_buffer_t *buffer, unsigned char *b, size_t len) {
  brook_buffer_t *memory = con->protocol_ctx;
  brook_http_request_t *req = (brook_http_request_t *)(void *)memory->data;

  // Parse the received data
  brook_http_err_e err = brook_http_parse(req, buffer, b, len);
  if (err != BROOK_HTTP_OK) {
    req->keep_alive = 0;
    brook_http_response_static(con, brook_http_errors[err].status, brook_http_errors[err].message);
    return;
  }
}

void brook_http_on_drain(brook_connection_t *con) {
  brook_buffer_t *memory = con->protocol_ctx;
  brook_http_request_t *req = (brook_http_request_t *)(void *)memory->data;

  // Because cliente hask or error append
  if (req->keep_alive == 0) {
    brook_connection_destroy(con);
    return;
  }

  brook_http_reset(con);
}

void brook_http_reset(brook_connection_t *con) {
  brook_buffer_t *memory = con->protocol_ctx;

  brook_http_request_t *req = (brook_http_request_t *)(void *)memory->data;
  memset(req, 0, sizeof(*req));

  req->keep_alive = 1;
}
