#ifndef BROOK_HTTP_RESPONSE_H
#define BROOK_HTTP_RESPONSE_H

#include "core/connection.h"
#include "http/request.h"
#include <stddef.h>
#include <stdint.h>

// All builders return a new heap string (caller must free) or NULL on error

char *brook_http_response_status_line(uint16_t status_code);
char *brook_http_response_header(const char *name, const char *value);

char *brook_http_response_header_content_length(size_t len);
char *brook_http_response_header_content_type(const char *type);
char *brook_http_response_header_connection(int keep_alive);
char *brook_http_response_header_location(const char *url);
char *brook_http_response_header_server(const char *name);
char *brook_http_response_header_cache_control(const char *value);

int brook_http_response_static(brook_http_request_t *req, uint16_t status_code, const char *message);

#endif
