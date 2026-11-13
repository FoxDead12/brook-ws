#ifndef POLL_H
#define POLL_H
#define POLL_MAX_FD 1024

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <poll.h>
#ifdef POLLRDHUP
#define POLL_EV_RDHUP POLLRDHUP
#else
#define POLL_EV_RDHUP 0
#endif
#define POLL_REV_CLOSE (POLLHUP | POLLERR | POLL_EV_RDHUP)

/*
 * This is the call backs the polling will call each time a event append
 * is the 1 layer the main of everthing
 */
typedef struct {
  void *ctx;
  int (*on_read)(void *ctx);
  int (*on_write)(void *ctx);
  int (*on_close)(void *ctx);
} poll_handlers_t;

/*
 * All "private" functions
 * */
void _poll_dispatch(int idx);

/*
 * All "public" functions avaliable to project
 * */
void poll_setup(void);
int poll_start(void);

int poll_add(int fd, poll_handlers_t handler);
void poll_remove(int fd);

void poll_add_events(int fd, short events);
void poll_remove_events(int fd, short events);

#endif // POLL_H
