#include "core/core.h"

/*
 * Method to create a socket binding to a port in system
 */
int brook_socket(int port) {

  int socket_fd;
  int bit = 1;
  struct sockaddr_in addr;

  // ignore erros when write to invalid socket
  signal(SIGPIPE, SIG_IGN);

  // Configure address of socket
  addr.sin_family = AF_INET; // only allow IPV4 connections
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // only allow localhost comunications

  // Create socket in memory
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    perror("socket problem when was been creating");
    return BROOK_ERROR;
  }

  // Set the socket was non block to system
  // this will make each call to im only acess the memory in kernel buffer
  if (fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
    perror("socket problem when set was non blocking");
    return BROOK_ERROR;
  }

  // Enable multi processos use the same port
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &bit, sizeof(int)) == -1) {
    perror("SO_REUSEPORT");
    return BROOK_ERROR;
  }

  // Enable server connect to same address
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &bit, sizeof(int)) == -1) {
    perror("SO_REUSEADDR");
    return BROOK_ERROR;
  }

  // TCP await to fill a package to send (make a biger frame to send data)
  if (setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &bit, sizeof(int)) == -1) {
    perror("TCP_NODELAY");
    return BROOK_ERROR;
  }

  // Bind the socket to port/address in system
  if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    return BROOK_ERROR;
  }

  // Enable system listem from port/address
  if (listen(socket_fd, SOMAXCONN) == -1) {
    perror("listen");
    return BROOK_ERROR;
  }

  return socket_fd;
}

/*
 * Method to close a socket in system
 */
int brook_socket_close(int socket) {
  close(socket);
  return BROOK_OK;
}
