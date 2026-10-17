#ifndef BROOK_CONFIG_H
#define BROOK_CONFIG_H

#include <stddef.h>

#include "core/types.h"

typedef struct {
  int port;
  int workers;
  char logger[1024];
} brook_config_t;

int brook_config(const char *config_file_path, brook_config_t *conf);
int brook_config_set_key(brook_config_t *conf, char *key, size_t key_size, char *value, int value_size);
int brook_config_file_read_line(int fd, char *buffer, int buffer_size);

#endif
