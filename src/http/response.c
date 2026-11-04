#include "http/response.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BROOK_HTTP_LINE_MAX 512

/*
 * Every builder returns a pointer to its own static buffer: valid until the next
 * call of the same builder, so queue/copy it before calling it again.
 */

static int brook_http_send(brook_connection_t *con, const char *piece);
static char *brook_http_header_into(char *buf, size_t size, const char *name, const char *value);
static const char *brook_http_reason(uint16_t status_code);

/*
 * Method will generate a static response
 */
int brook_http_response_static(brook_connection_t *con, uint16_t status_code, const char *message) {

  static char body[BROOK_HTTP_LINE_MAX];
  size_t body_len = snprintf(body, sizeof body, "{\"title\":\"%s\",\"status\":%u,\"detail\":\"%s\"}", brook_http_reason(status_code), status_code, message);

  if (brook_http_send(con, brook_http_response_status_line(status_code)) == BROOK_ERROR ||
      brook_http_send(con, brook_http_response_header_content_type("application/problem+json")) == BROOK_ERROR ||
      brook_http_send(con, brook_http_response_header_content_length(body_len)) == BROOK_ERROR ||
      brook_http_send(con, brook_http_response_header_connection(0)) == BROOK_ERROR ||
      brook_http_send(con, "\r\n") == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  if (body_len > 0) {
    return brook_connection_send(con, (const unsigned char *)body, body_len);
  }

  return BROOK_OK;
}

/*
 * Status line: "HTTP/1.1 200 OK\r\n"
 */
char *brook_http_response_status_line(uint16_t status_code) {
  static char buf[64];
  int n = snprintf(buf, sizeof buf, "HTTP/1.1 %u %s\r\n", status_code, brook_http_reason(status_code));
  return (n < 0 || (size_t)n >= sizeof buf) ? NULL : buf;
}

char *brook_http_response_header(const char *name, const char *value) {
  static char buf[BROOK_HTTP_LINE_MAX];
  return brook_http_header_into(buf, sizeof buf, name, value);
}

/*
 * Queue one piece on the connection, NULL piece means the builder failed
 */
static int brook_http_send(brook_connection_t *con, const char *piece) {
  if (piece == NULL) {
    return BROOK_ERROR;
  }
  return brook_connection_send(con, (const unsigned char *)piece, strlen(piece));
}

/*
 * Write "name: value\r\n" into buf, returns buf or NULL on invalid input / truncation
 */
static char *brook_http_header_into(char *buf, size_t size, const char *name, const char *value) {
  if (name == NULL || value == NULL || name[0] == '\0') {
    return NULL;
  }

  // Proteção contra injeção: \r ou \n partiriam a resposta ou criariam cabeçalhos falsos
  if (strpbrk(name, "\r\n:") != NULL || strpbrk(value, "\r\n") != NULL) {
    brook_debug("http response error: invalid chars in header %s\n", name);
    return NULL;
  }

  int n = snprintf(buf, size, "%s: %s\r\n", name, value);
  if (n < 0 || (size_t)n >= size) {
    brook_debug("http response error: no space for header %s\n", name);
    return NULL;
  }

  return buf;
}

static const char *brook_http_reason(uint16_t status_code) {
  switch (status_code) {
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 204:
    return "No Content";
  case 301:
    return "Moved Permanently";
  case 302:
    return "Found";
  case 304:
    return "Not Modified";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 408:
    return "Request Timeout";
  case 413:
    return "Payload Too Large";
  case 414:
    return "URI Too Long";
  case 431:
    return "Request Header Fields Too Large";
  case 500:
    return "Internal Server Error";
  case 501:
    return "Not Implemented";
  case 503:
    return "Service Unavailable";
  case 505:
    return "HTTP Version Not Supported";
  default:
    return "Unknown";
  }
}

/*
 * Header helpers
 */

char *brook_http_response_header_content_length(size_t len) {
  static char buf[64];
  char value[24];
  snprintf(value, sizeof value, "%zu", len);
  return brook_http_header_into(buf, sizeof buf, "Content-Length", value);
}

char *brook_http_response_header_content_type(const char *type) {
  static char buf[BROOK_HTTP_LINE_MAX];
  return brook_http_header_into(buf, sizeof buf, "Content-Type", type);
}

char *brook_http_response_header_connection(int keep_alive) {
  static char buf[64];
  return brook_http_header_into(buf, sizeof buf, "Connection", keep_alive ? "keep-alive" : "close");
}

char *brook_http_response_header_location(const char *url) {
  static char buf[BROOK_HTTP_LINE_MAX];
  return brook_http_header_into(buf, sizeof buf, "Location", url);
}

char *brook_http_response_header_server(const char *name) {
  static char buf[BROOK_HTTP_LINE_MAX];
  return brook_http_header_into(buf, sizeof buf, "Server", name);
}

char *brook_http_response_header_cache_control(const char *value) {
  static char buf[BROOK_HTTP_LINE_MAX];
  return brook_http_header_into(buf, sizeof buf, "Cache-Control", value);
}
