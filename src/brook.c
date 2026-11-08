#include "core/config.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/process.h"
#include "core/socket.h"
#include "core/types.h"
#include <errno.h>

brook_config_node_t *conf = NULL;
brook_logger_t logger = {0};
brook_process_t process = {0};

/*
 * Here will start the brook websocket server
 */
int main(void) {

  // Initialize the configuration of server from config file
  conf = brook_config_load("brook-config.conf");
  if (conf == NULL) {
    return BROOK_ERROR;
  }

  // Required keys, the tree is alive until the end of main so the pointers stay valid
  const char *logger_path = brook_config_get_str(conf, "logger", NULL);
  int port = brook_config_get_int(conf, "port", -1);

  if (logger_path == NULL || port < 0) {
    fprintf(stderr, "config: 'port' and 'logger' are required\n");
    brook_config_node_free(conf);
    return BROOK_ERROR;
  }

  // Initialize logger
  logger.path = logger_path;
  if (brook_logger(&logger) == BROOK_ERROR) {
    brook_config_node_free(conf);
    return BROOK_ERROR;
  }

  // Only now the logger is open, so we can log what was loaded from the config file
  brook_config_debug(conf);

  // Initialize server
  process.socket_fd = brook_socket(port);
  if (process.socket_fd == -1) {
    brook_log(&logger, L_ERROR, "Error opening socket: %s\n", errno);
    brook_config_node_free(conf);
    return BROOK_ERROR;
  }

  // TODO: for now is dev only one process
  brook_process();

  brook_config_node_free(conf);
  return BROOK_OK;
}
