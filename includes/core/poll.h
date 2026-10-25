#ifndef BROOK_POLL_H
#define BROOK_POLL_H

#define POLL_FDS_MAX 1024

typedef struct {
  void (*on_read)(void *ctx);
  void (*on_write)(void *ctx);
  void (*on_error)(void *ctx); // POLLERR | POLLHUP | POLLNVAL
  void *ctx;
} brook_handler_t;

void brook_poll_loop(void);
void brook_poll_dispatch(int slot);
int brook_poll_next_slot(void);
int brook_poll_add(int fd, short events, brook_handler_t handler);
int brook_poll_remove(int slot);
void brook_poll_set_ctx(int slot, void *ctx);

#endif
