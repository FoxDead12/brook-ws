#ifndef BROOK_BUFFER_H
#define BROOK_BUFFER_H

typedef struct brook_buffer_s {
  int size;
  int len;
  int free;
  unsigned char *data;
  struct brook_buffer_s *next;
  int in_use;
} brook_buffer_t;

brook_buffer_t *brook_buffer_get_free(brook_buffer_t **head);
int brook_buffer_destroy(brook_buffer_t *head);

#endif
