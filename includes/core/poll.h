#ifndef POLL_H
#define POLL_H
#define POLL_MAX_FD 1024

/*
 * This is the call backs the polling will call each time a event append
 * is the 1 layer the main of everthing
 */
typedef struct {
  void *ctx;
  int (*on_read)(void *ctx);
  int (*on_write)(void *ctx);
  int (*on_open)(void *ctx);
  int (*on_close)(void *ctx);
} poll_handlers_t;

/*
 * All "private" functions
 * */
void _poll_dispatch(int idx);

/*
 * All "public" functions avaliable to project
 * */

int poll_start(void);

int poll_add(int fd, poll_handlers_t handler);
void poll_remove(int slot);

void poll_add_events(int slot, short events);
void poll_remove_events(int slot, short events);

#endif // POLL_H
