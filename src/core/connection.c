#include "core/connection.h"
#include "core/config.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/socket.h"
#include "core/types.h"
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Method used when server receive a new TCP connection
 */
void brook_connection_accept(void *ctx) {
  (void)ctx;

  int fd;
  struct sockaddr_in address;
  socklen_t len = sizeof(address);

  // Protect a process accept more connections than can handle
  if (process.nfds >= POLL_FDS_MAX) {
    // TODO: i can disable the event of read of server socket to disable events of accept event
    brook_debug("accept skipped, process is full nfds=%d max=%d\n", process.nfds, POLL_FDS_MAX);
    return;
  }

  // Accept connection
  fd = brook_socket_accept(process.socket_fd, (struct sockaddr *)&address, &len);
  if (fd == BROOK_DONE) {
    brook_debug("accept returned nothing (another worker took it or error)\n");
    return;
  }

  // Add the new connection in process data
  int idx = process.nfds; // make this because is a counter but arrays are lower starting from 0
  process.fds[idx] = (struct pollfd){.fd = fd, .events = POLLIN, .revents = 0};
  process.conns[idx] = (brook_connection_t){.fd = fd, .port = ntohs(address.sin_port)};
  inet_ntop(AF_INET, &address.sin_addr, process.conns[idx].ip, INET_ADDRSTRLEN);
  process.handlers[idx] = (brook_handler_t){.on_read = brook_connection_read, .on_write = NULL, .on_error = NULL, .ctx = &process.conns[idx]};
  process.nfds++;

  brook_debug("new connection fd=%d slot=%d from %s:%d nfds=%d\n", fd, idx, process.conns[idx].ip, process.conns[idx].port, process.nfds);
  return;
}

/*
 * Method when a socket/connection send data and we need read
 */
void brook_connection_read(void *ctx) {

  brook_connection_t *con = (brook_connection_t *)ctx;

  char buffer[1024] = {0};
  read(con->fd, buffer, sizeof(buffer));

  printf("buffer: %s\n", buffer);

  return;
}
