#include "job/queue.h"
#include "beanstalkclient.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/types.h"
#include <unistd.h>

static void brook_job_queue_on_error(bsc *con, bsc_error_t error) {
  // TODO: need kill the process or reconnect
  (void)con;
  brook_log(&logger, L_ERROR, "beanstalkd error: %d", (int)error);
}

/*
 * Create a socket/fd related to beanstalkd
 */
int brook_job_queue_create(const char *host, const char *port) {

  char errorstr[BSC_ERRSTR_LEN] = {0};
  const char *tube = BSC_DEFAULT_TUBE;

  bsc *con = bsc_new_w_defaults(host, port, tube, brook_job_queue_on_error, errorstr);
  if (con == NULL) {
    brook_log(&logger, L_ERROR, "beanstalkd connect %s:%s failed: %s", host, port, errorstr);
    return BROOK_ERROR;
  }

  // initialte the daulft values
  process.jobs.fd = con->fd;
  process.jobs.client = con;
  process.jobs.njobs = 0;

  brook_debug("beanstalkd connected %s:%s tube=%s fd=%d", host, port, tube, con->fd);
  return BROOK_OK;
}

void brook_job_queue_on_read(void *ctx) {
  bsc *client = (bsc *)ctx;
  bsc_read(client);
}

void brook_job_queue_on_write(void *ctx) {
  bsc *client = (bsc *)ctx;
  bsc_write(client);
}
