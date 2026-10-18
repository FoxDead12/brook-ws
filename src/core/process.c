#include "core/process.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/types.h"
#include <stdio.h>
#include <unistd.h>

/*
 * Method to start a process worker of a brook-ws
 */
int brook_process(void) {

  // Store the pid of process
  process.pid = getpid();
  brook_log(&logger, L_INFO, "Process will execute the initial setup\n");

  return BROOK_OK;
}
