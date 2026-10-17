#ifndef _BROOK_CORE_H
#define _BROOK_CORE_H

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <regex.h>
#include <stddef.h>

// Static variables define to help the developer
#define BROOK_OK 0
#define BROOK_ERROR -1
#define BROOK_DONE -2

#include "config.h"
#include "logger.h"
#include "socket.h"

#endif
