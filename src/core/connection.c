#include "core/connection.h"
#include "core/types.h"

/*
 * Here is the Layer 2. Here we are in connection level, the con will controll is state and
 * if necessary the poll slot of con.
 */

/*
 * On each new connection enter here
 */
int connection_accept(void *ctx) {
  (void)ctx;

  return BROOK_OK;
}
