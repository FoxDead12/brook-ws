#include "core/globals.h"
#include "core/logger.h"
#include "core/process.h"
#include "core/socket.h"
#include "core/types.h"
#include <errno.h>

brook_config_t conf = {0};
brook_logger_t logger = {0};
brook_process_t process = {0};

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

  // Initialize server
  process.socket_fd = brook_socket(conf.port);
  if (process.socket_fd == -1) {
    brook_log(&logger, L_ERROR, "Error opening socket: %s\n", errno);
    return BROOK_ERROR;
  }

  // TODO: for now is dev only one process
  brook_process();

  return BROOK_OK;
}
