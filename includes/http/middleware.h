#ifndef BROOK_HTTP_MIDDLEWARE_H
#define BROOK_HTTP_MIDDLEWARE_H

#include "http/request.h"

void brook_http_middleware_on_call(brook_http_request_t *req);
void brook_http_middleware_on_finish(brook_http_request_t *req);

#endif
