#include "core/logger.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define LOGGER_PATH_MAX 512

/* Private logger state, only this file knows it */
static struct {
  int fd;
  int current_day;
  pid_t pid;
  char path[LOGGER_PATH_MAX];
} _logger = {.fd = -1};

static const char *logger_level_name(logger_level_t level) {
  switch (level) {
  case LOGGER_DEBUG:
    return "DEBUG";
  case LOGGER_VERBOSE:
    return "VERBOSE";
  case LOGGER_INFO:
    return "INFO";
  case LOGGER_WARN:
    return "WARN";
  case LOGGER_ERROR:
    return "ERROR";
  }
  return "UNKNOWN";
}

/*
 * Open (or create) the log file of the day. Returns the fd or -1 on error.
 * Does not touch the logger state, so callers decide when to switch.
 */
static int logger_open_file(const struct tm *tm) {
  char file_name[64];
  char file_path[LOGGER_PATH_MAX + 64];

  strftime(file_name, sizeof file_name, "brook-ws-%Y-%m-%d.log", tm);

  int n = snprintf(file_path, sizeof file_path, "%s/%s", _logger.path, file_name);
  if (n < 0 || (size_t)n >= sizeof file_path) {
    errno = ENAMETOOLONG;
    return -1;
  }

  return open(file_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
}

/*
 * Create the log directory and open the first log file
 */
int logger_init(const char *path) {
  time_t t = time(NULL);
  struct tm tm;

  if (!path || strlen(path) >= sizeof _logger.path) {
    fprintf(stderr, "Error: invalid log path\n");
    return BROOK_ERROR;
  }

  if (!localtime_r(&t, &tm))
    return BROOK_ERROR;

  // The path is copied, so the caller does not need to keep it alive
  snprintf(_logger.path, sizeof _logger.path, "%s", path);

  if (mkdir(_logger.path, 0755) == -1 && errno != EEXIST) {
    perror("Error creating log directory");
    return BROOK_ERROR;
  }

  int fd = logger_open_file(&tm);
  if (fd == -1) {
    perror("Error creating file log");
    return BROOK_ERROR;
  }

  _logger.fd = fd;
  _logger.current_day = tm.tm_mday;
  _logger.pid = getpid();

  return BROOK_OK;
}

/*
 * Close the log file
 */
void logger_close(void) {
  if (_logger.fd != -1) {
    close(_logger.fd);
    _logger.fd = -1;
  }
}

/*
 * Record a log line (one write() per line)
 */
int logger_log(logger_level_t level, const char *fmt, ...) {
  // Not initialized (or already closed): never write to a stray fd
  if (_logger.fd == -1)
    return BROOK_ERROR;

  time_t t = time(NULL);
  struct tm tm;

  if (!localtime_r(&t, &tm))
    return BROOK_ERROR;

  // New day: open the new file first, only then close the old one
  if (tm.tm_mday != _logger.current_day) {
    int fd = logger_open_file(&tm);
    if (fd != -1) {
      close(_logger.fd);
      _logger.fd = fd;
      _logger.current_day = tm.tm_mday;
    }
    // on failure keep writing to the old file and retry on the next log
  }

  char buf[2048];
  int n = snprintf(buf, sizeof buf, "[%02d-%02d-%04dT%02d:%02d:%02d][%d][%s] ",
                   tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour,
                   tm.tm_min, tm.tm_sec, (int)_logger.pid,
                   logger_level_name(level));
  if (n < 0)
    return BROOK_ERROR;

  va_list args;
  va_start(args, fmt);
  int m = vsnprintf(buf + n, sizeof buf - n, fmt, args);
  va_end(args);

  if (m < 0)
    return BROOK_ERROR;
  n += m;

  // Truncated message: keep room for the newline
  if (n > (int)sizeof buf - 2)
    n = sizeof buf - 2;
  buf[n++] = '\n';

  return write(_logger.fd, buf, n) == n ? BROOK_OK : BROOK_ERROR;
}
