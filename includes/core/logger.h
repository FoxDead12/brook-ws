#ifndef LOGGER_H
#define LOGGER_H

#include "core/types.h"

typedef enum {
  LOGGER_DEBUG,
  LOGGER_VERBOSE,
  LOGGER_INFO,
  LOGGER_WARN,
  LOGGER_ERROR
} logger_level_t;

int logger_init(const char *path);
void logger_close(void);
int logger_log(logger_level_t level, const char *fmt, ...);

/*
 * Log macros. Debug logs only exist when compiled with -DBROOK_DEBUG
 * (make debug), otherwise the call (and its arguments) is removed by
 * the preprocessor.
 */
#ifdef BROOK_DEBUG
#define logger_debug(...) logger_log(LOGGER_DEBUG, __VA_ARGS__)
#else
#define logger_debug(...) ((void)0)
#endif

#define logger_verbose(...) logger_log(LOGGER_VERBOSE, __VA_ARGS__)
#define logger_info(...) logger_log(LOGGER_INFO, __VA_ARGS__)
#define logger_warn(...) logger_log(LOGGER_WARN, __VA_ARGS__)
#define logger_error(...) logger_log(LOGGER_ERROR, __VA_ARGS__)

#endif
