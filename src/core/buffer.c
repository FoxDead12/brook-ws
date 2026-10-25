#include "core/buffer.h"
#include "core/globals.h"
#include "core/logger.h"
#include "core/pool.h"
#include "core/types.h"
#include <stdio.h>

/*
 * Method to from a nodes/buffer list get a free one
 * if dont find one will add a new one
 */
brook_buffer_t *brook_buffer_get_free(brook_buffer_t **head) {

  brook_buffer_t *buffer = *head;
  brook_buffer_t *last = NULL;

  // Move pointer to a free buffer in list is possible not found
  while (buffer != NULL && buffer->free == 0) {
    last = buffer;
    buffer = buffer->next;
  }

  // In current list dont exist a free buffer
  if (buffer == NULL) {

    // Get a new buffer from pool
    brook_buffer_t *node = brook_pool_malloc(process.pool_head);
    if (node == NULL) {
      return NULL;
    }

    buffer = node;

    if (last != NULL) {
      last->next = buffer;
    } else {
      *head = buffer;
    }
  }

  return buffer;
}

int brook_buffer_destroy(brook_buffer_t *head) {

  brook_buffer_t *buffer = head;
  brook_buffer_t *last = NULL;

  // Will iterate all buffers in list
  while (buffer != NULL) {
    last = buffer;
    buffer = buffer->next;
    last->next = NULL;
    brook_pool_free(process.pool_head, last);
  }

  return BROOK_OK;
}
