#ifndef SOCKET_H
#define SOCKET_H

#include "core/types.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int core_socket_bind(int port);

#endif // !SOCKET_H
