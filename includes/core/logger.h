#ifndef BROOK_LOGGER_H
#define BROOK_LOGGER_H

#include "core/config.h"
#include "core/types.h"

typedef struct {
  int fd;
  int current_day;
  const char *path;
} brook_logger_t;

typedef enum {
  L_DEBUG,
  L_VERBOSE,
  L_INFO,
  L_WARN,
  L_ERROR
} brook_logger_enum;

static inline const char *brook_logger_type_name(brook_logger_enum type) {
  switch (type) {
  case L_DEBUG:
    return "DEBUG";
  case L_VERBOSE:
    return "VERBOSE";
  case L_INFO:
    return "INFO";
  case L_WARN:
    return "WARN";
  case L_ERROR:
    return "ERROR";
  }
  return "UNKNOWN";
}

int brook_logger(brook_logger_t *logger);
int brook_log(brook_logger_t *logger, brook_logger_enum type, const char *fmt, ...);

/*
 * Debug logs only exist when compiled with -DBROOK_DEBUG (make debug),
 * otherwise the call (and its arguments) is removed by the preprocessor.
 * Needs the global `logger` in scope (core/globals.h).
 */
#ifdef BROOK_DEBUG
#define brook_debug(...) brook_log(&logger, L_DEBUG, __VA_ARGS__)
#else
#define brook_debug(...) ((void)0)
#endif

#endif
