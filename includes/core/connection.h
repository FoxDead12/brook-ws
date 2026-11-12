#ifndef CONNECTION_H
#define CONNECTION_H
#include "core/poll.h"
#include <arpa/inet.h>
#include <stddef.h>
#define MAX_CONNS (POLL_MAX_FD - 8)

struct connection_s;

typedef struct {
  int (*on_data)(struct connection_s *con);
  int (*on_write)(struct connection_s *con);
  int (*on_close)(struct connection_s *con);
} protocol_t;

/*
 * The struct of a connection
 */
typedef struct connection_s {
  int fd;
  int port;
  char ip[INET_ADDRSTRLEN];

  // the connection will caller someone whe receive events
  protocol_t *protocol;
  void *protocol_ctx;
} connection_t;

/*
 * Struct will store all existent connections in process/worker
 */
typedef struct {
  connection_t connections[MAX_CONNS];
  connection_t *free[MAX_CONNS];
  size_t free_count;
} connection_manager_t;

/*
 * Srtruct of listner
 */
typedef struct {
  int fd;
  int accepting;
} listner_t;

/*
 * All "private" functions
 * */
connection_t *_connection_acquire(void);
void _connection_release(connection_t *con);

void _lister_stop_accept(void);
void _listner_start_accept(void);

/*
 * All "public" functions avaliable to project
 * */

int connection_setup(int fd, protocol_t *protocol);
int connection_accept(void *ctx);

int connection_on_read(void *ctx);
int connection_on_write(void *ctx);
int connection_on_close(void *ctx);

#endif // !CONNECTION_H
