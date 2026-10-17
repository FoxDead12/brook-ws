#include "core/config.h"
#include "core/logger.h"
#include "core/process.h"
#include "core/types.h"

brook_logger_t logger;
brook_config_t conf;

/*
 * Here will start the brook websocket server
 */
int main(void) {

  // Initialize the configuration of server from config file
  if (brook_config("brook-config.conf", &conf) == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  // Initialize logger
  logger.path = conf.logger;
  if (brook_logger(&logger) == BROOK_ERROR) {
    return BROOK_ERROR;
  }

  return BROOK_OK;
}
