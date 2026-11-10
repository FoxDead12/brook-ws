#include "core/process.h"
#include "core/poll.h"
#include "core/socket.h"

/*
 * The processor is the main of each process/worker
 * here we join and merge all layers will exist and set the necessary 
 * configurations to code
 */

int core_process_start (void) {

  int socket = core_socket_bind(3001);

  printf("cirei um fd: %d\n", socket);

}
