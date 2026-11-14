#include "core/buffer.h"
#include "core/pool.h"
#include <assert.h>
#include <stddef.h>

/*
 * will get a buffer were contain space to write more in memory
 * */
buffer_t *chain_acquire(chain_t *chain) {

  // check if tail contain space to be used
  if (chain->tail && chain->tail->len < BUFFER_SIZE) {
    return chain->tail;
  }

  // need get one from pool and add to list
  buffer_t *new = pool_acquire(chain->pool);

  // is possible don't get the buffer
  if (new == NULL) {
    return NULL;
  }

  // if tail is null the header is null to
  if (chain->tail == NULL) {
    chain->head = new;
  } else {
    // update the current last to point to new last
    chain->tail->next = new;
  }

  // make the new last the real last of list
  chain->tail = new;

  // increment the buffers count
  chain->count++;

  return new;
}

/*
 * to indicate when a buffer store data in memory, move the positions of counters
 */
void chain_commit(chain_t *chain, size_t n) {
  assert(chain->tail != NULL);
  assert(chain->tail->len + n <= BUFFER_SIZE);
  chain->tail->len += n;
  chain->total += n;
}

void chain_consume(chain_t *chain, size_t n) {

  assert(n <= chain->total);

  // remove the bytes discarded
  chain->total -= n;

  while (chain->head && n > 0) {

    buffer_t *buffer = chain->head;

    // get the diference
    size_t free = buffer->len - buffer->pos;
    size_t taken = free;

    // if the number bytes to taken is bigger then n became n
    if (taken > n) {
      taken = n;
    }

    buffer->pos += taken;
    n -= taken;

    // if now the pos and len are the same we can discard
    if (buffer->pos == buffer->len) {

      // move the head to next
      chain->head = chain->head->next;

      // if header is null now the tail is null to
      if (chain->head == NULL) {
        chain->tail = NULL;
      }

      // release to pool
      pool_release(chain->pool, buffer);

      // remove the numb of blocks
      chain->count--;
    }
  }
}
