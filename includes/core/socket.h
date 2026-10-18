#ifndef BROOK_SOCKET_H
#define BROOK_SOCKET_H

#include "core/types.h"
#include <sys/socket.h>

int brook_socket(int port);
int brook_socket_accept(int fd, struct sockaddr *address, socklen_t *len);
int brook_socket_close(int socket);

#endif
