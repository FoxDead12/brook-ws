#include "core/logger.h"
#include "core/process.h"
#include "core/types.h"

/*
 * Entry point of brook webserver
 * welcome to the world of brook
 * Peac and love
 * */
int main(void) {

#if defined(__linux__)
  /* Linux */
  // init the logger for this proccess
  logger_init("/var/log/brook/");
#elif defined(__APPLE__) && defined(__MACH__)
  /* macOS */
  // init the logger for this proccess
  logger_init("/Users/dxavier/Library/Logs/BrookWs/");
#else
#error "unsupported platform"
#endif

  // run a worker directly in main thread for now
  process_start();

  return BROOK_OK;
}
