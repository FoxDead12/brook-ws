#ifndef MODULES_H
#define MODULES_H

typedef struct {
  int (*on_execute)(http_t *http);
  int (*on_close)(http_t *http);
} module_t;

typedef struct {
  module_t *modules[MODULES_MAX];
  size_t count;
} module_manager_t;

#endif // !MODULES_H
