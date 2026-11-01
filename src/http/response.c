#include "http/response.h"
#include "core/types.h"
#include <stdint.h>

/*
 * Method will generate a static response
 */
int brook_http_response_static(brook_connection_t *con, uint16_t status_code, const char *message) {

  return BROOK_OK;
}

int brook_http_response_header(brook_http_response_t *res, const char *name, const char *value) {

  // Proteção contra injeção: \r ou \n partiriam a resposta ou criariam cabeçalhos falsos
  if (strpbrk(name, "\r\n:") != NULL || strpbrk(value, "\r\n") != NULL) {
    brook_debug("http response error: invalid chars in header %s\n", name);
    return BROOK_ERROR;
  }

  size_t left = sizeof(res->head) - res->len;
  int n = snprintf(res->head + res->len, left, "%s: %s\r\n", name, value);

  // snprintf devolve o tamanho que queria escrever, se n >= left ficou truncado
  if (n < 0 || (size_t)n >= left) {
    brook_debug("http response error: no space for header %s\n", name);
    return BROOK_ERROR;
  }

  res->len += n;
  return BROOK_OK;
}

int brook_http_response_header_content_length(brook_http_response_t *res, size_t len) {
  char value[24];
  snprintf(value, sizeof value, "%zu", len);
  return brook_http_response_header(res, "Content-Length", value);
}

int brook_http_response_header_content_type(brook_http_response_t *res, const char *type) {
  return brook_http_response_header(res, "Content-Type", type);
}

int brook_http_response_header_connection(brook_http_response_t *res, int keep_alive) {
  return brook_http_response_header(res, "Connection", keep_alive ? "keep-alive" : "close");
}

int brook_http_response_header_location(brook_http_response_t *res, const char *url) {
  return brook_http_response_header(res, "Location", url);
}

int brook_http_response_header_server(brook_http_response_t *res, const char *name) {
  return brook_http_response_header(res, "Server", name);
}

int brook_http_response_header_cache_control(brook_http_response_t *res, const char *value) {
  return brook_http_response_header(res, "Cache-Control", value);
}
