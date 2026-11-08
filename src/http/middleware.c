
#include "http/middleware.h"

static const brook_middleware_fn chain[] = {
    brook_job_enqueue, // por agora só este
                       // brook_gatekeeper,        // depois entra aqui, antes do enqueue
};

void brook_http_middleware_on_call(brook_http_request_t *req) {
  // the http request will cal this method now we nee manage

  req->middleware
      chain[req->middleware](req);

  for (size_t i = 0; i < sizeof chain / sizeof *chain; i++) {
    if (chain[i](req) == BROOK_ERROR) {
      return;
    }
  }

  brook_http_middleware_set(brook_session_valid_job, req);
  brook_http_middleware_set(brook_job_enqeueu_job, req);
}

void brook_http_middleware_on_finish(brook_http_request_t *req) {
  // i dont now how will call
}
