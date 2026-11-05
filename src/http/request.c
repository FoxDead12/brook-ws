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

  // wee will set a memory/buffer to work whith static data  of protocol
  brook_buffer_t *memory = brook_buffer_get_free(&con->protocol_ctx);
  if (memory == NULL) {
    brook_log(&logger, L_ERROR, "Can't allocate http context for fd=%d\n", con->fd);
    brook_connection_destroy(con);
    return;
  }

  // We need contain this check before compile
  _Static_assert(sizeof(brook_http_t) <= BROOK_POOL_BLOCK_SIZE, "request does not fit in a pool block");

  // "allocate the parser in memory of http protocol"
  brook_http_t *http = (brook_http_t *)memory->data;
  memset(http, 0, sizeof(*http));
}

/*
 * Method called when connection change protocol or is closed
 */
void brook_http_on_close(brook_connection_t *con) {
  brook_buffer_destroy(con->protocol_ctx);
  con->protocol_ctx = NULL;
}

/*
 * Method called each time connection read data
 */
void brook_http_on_data(brook_connection_t *con, brook_buffer_t *buffer, unsigned char *b, size_t len) {
  brook_buffer_t *memory = con->protocol_ctx;
  brook_http_t *http = (brook_http_t *)(void *)memory->data;
  brook_http_parser_t *parser = &http->parser;
  brook_http_request_t *req = &http->request;

  // start the request parse will run olny one time per request
  if (parser->state == s_req_start) {
    brook_debug("http new request fd=%d len=%zu\n", con->fd, len);
    memset(req, 0, sizeof(*req));
    req->keep_alive = 1;
    req->con = con;
  }

  // parser the buffer data looking for a request
  brook_http_err_e error = brook_http_parse(http, buffer, b, len);

  // If error append will close the connection because is a not valid request
  if (error != BROOK_HTTP_OK) {
    // stop read because will close the socket
    req->keep_alive = 0;
    brook_connection_read_stop(con);
    if (brook_http_response_static(req, brook_http_errors[error].status, brook_http_errors[error].message) == BROOK_ERROR) {
      brook_connection_destroy(con);
    }
    return;
  }

  // Request parser will restart probably and disable read
  if (parser->state == s_req_done) {
    brook_debug("http request done fd=%d pending=%s offset=%u\n", con->fd, parser->cursor ? "yes" : "no", parser->offset);
    brook_connection_read_stop(con);
    brook_http_parser_reset(parser);
  }
}

/*
 * Method called each time the out buffers on write is null/empty
 */
void brook_http_on_drain(brook_connection_t *con) {
  brook_buffer_t *memory = con->protocol_ctx;
  brook_http_t *http = (brook_http_t *)(void *)memory->data;
  brook_http_parser_t *parser = &http->parser;
  brook_http_request_t *req = &http->request;

  if (req->keep_alive == 0) {
    brook_connection_destroy(con);
    return;
  }

  // check if we dont contain pendent values to read
  if (parser->cursor != NULL) {
    brook_buffer_t *cur = parser->cursor;
    size_t off = parser->offset;
    parser->cursor = NULL;
    parser->offset = 0;
    brook_debug("http resume pending bytes=%zu offset=%zu fd=%d\n", (size_t)(cur->len - off), off, con->fd);
    brook_http_on_data(con, cur, cur->data + off, cur->len - off);
  }

  brook_connection_read_start(con);
}

void brook_http_parser_reset(brook_http_parser_t *parser) {
  parser->state = s_req_start;
  parser->match_index = 0;
  parser->sub_state = 0;
  parser->header_size = 0;
  parser->header_match_mask = 0;
  parser->header_state = h_general;
}
