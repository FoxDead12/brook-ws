#include "core/logger.h"
#include "core/process.h"
#include "core/types.h"

/*
 * Entry point of brook webserver
 * welcome to the world of brook
 * Peac and love
 * */
int main(void) {

  // init the logger for this proccess
  logger_init("/Users/dxavier/Library/Logs/BrookWs/");

  // run a worker directly in main thread for now
  process_start();

  return BROOK_OK;
}
