#include "core/logger.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/*
 * Method will execute the logger and enable the process log
 */
int brook_logger(brook_logger_t *logger) {

  int fd;
  time_t t;
  char file_name[64];
  char file_path[1024];
  struct tm *time_info;
  const char *logger_path = logger->path;

  // Create the directory to logger path
  if (mkdir(logger_path, 0755) == -1) {
    if (errno != EEXIST) {
      perror("Error creating log directory");
      return BROOK_ERROR;
    }
  }

  // Create the logger file name by date
  t = time(NULL);
  time_info = localtime(&t);
  strftime(file_name, sizeof(file_name), "brook-ws-%Y-%m-%d.log", time_info);

  // Set the current day in struct
  logger->current_day = time_info->tm_mday;

  // Create path to destiny file
  snprintf(file_path, sizeof(file_path), "%s/%s", logger_path, file_name);

  // Open file
  fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  if (fd == -1) {
    perror("Error creating file log");
    return BROOK_ERROR;
  }

  if (logger->fd > 0) {
    close(fd);
  }

  logger->fd = fd;

  return BROOK_OK;
}

/*
 * Method to record each log called in code
 */
int brook_log(brook_logger_t *logger, brook_logger_enum type, const char *fmt, ...) {

  time_t t;
  struct tm *time_info;

  // Set values of time
  t = time(NULL);
  time_info = localtime(&t);

  // We regenerate new file if the current day of log change
  if (time_info->tm_mday != logger->current_day) {
    brook_logger(logger);
  }

  va_list args;
  va_start(args, fmt);

  t = time(NULL);
  time_info = localtime(&t);

  dprintf(logger->fd, "[%02d-%02d-%04dT%02d:%02d:%02d][%d][%s] ",
          time_info->tm_mday, time_info->tm_mon + 1, time_info->tm_year + 1900,
          time_info->tm_hour, time_info->tm_min, time_info->tm_sec,
          getpid(), brook_logger_type_name(type));

  vdprintf(logger->fd, fmt, args);

  va_end(args);
  return BROOK_OK;
}
