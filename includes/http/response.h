#ifndef BROOK_HTTP_RESPONSE_H
#define BROOK_HTTP_RESPONSE_H

#include "core/connection.h"
#include "http/response.h"

int brook_http_response_static(brook_connection_t *con, uint16_t status_code, const char *message);

int brook_http_response_header(brook_http_response_t *res, const char *name, const char *value);

int brook_http_response_header_content_length(brook_http_response_t *res, size_t len);
int brook_http_response_header_content_type(brook_http_response_t *res, const char *type);
int brook_http_response_header_connection(brook_http_response_t *res, int keep_alive);
int brook_http_response_header_location(brook_http_response_t *res, const char *url);
int brook_http_response_header_server(brook_http_response_t *res, const char *name);
int brook_http_response_header_cache_control(brook_http_response_t *res, const char *value);

#endif
