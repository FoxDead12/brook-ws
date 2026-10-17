#include "core/config.h"
#include "core/core.h"
#include <_regex.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
 * Method wo will read and store the config of server from file
 */
int brook_config(const char *config_file_path) {

  int file_fd;
  regex_t reegex;
  regmatch_t match[3];
  char buffer[1024];
  brook_config_t config;

  // Open destination file in read only mode
  file_fd = open(config_file_path, O_RDONLY, 0);

  if (file_fd == -1) {
    perror("file problem when open to read");
    return BROOK_ERROR;
  }

  // Create regex in memory
  if (regcomp(&reegex, "([[:alnum:]_]+):[[:space:]]*([[:alnum:]_]+)", REG_EXTENDED) != 0) {
    perror("regex");
    return BROOK_ERROR;
  }

  while (1) {

    // Read a intere line of file
    int b = brook_config_file_read_line(file_fd, buffer, sizeof(buffer));
    if (b == 0) {
      continue;
    }

    // Line dont match the regex
    if (regexec(&reegex, buffer, 3, match, 0) != 0) {
      fprintf(stderr, "invalid line in conf file: %s\n", buffer);
      return BROOK_ERROR;
    }

    printf("read %d: %.*s\n", b, b, buffer);

    char *key = buffer + match[1].rm_so;
    char *value = buffer + match[2].rm_so;
    brook_config_set_key(&config, key, match[1].rm_eo - match[1].rm_so, value, match[2].rm_eo - match[2].rm_so);
  }

  return BROOK_OK;
}

int brook_config_set_key(brook_config_t *conf, char *key, size_t key_size, char *value, int value_size) {

  // Set all possible keys defined in config file
  const char *port = "port";
  const char *workers = "workers";

  char *value_end = value + value_size;

  // Wil set the value fo each value of struct
  if (strlen(port) == key_size && strncmp(key, port, key_size) == 0) {
    conf->port = strtol(value, &value_end, 10);
  } else if (strlen(workers) == key_size && strncmp(key, workers, key_size) == 0) {
    conf->workers = strtol(value, &value_end, 10);
  }

  return BROOK_OK;
}

/*
 * Method to read from file/fd a intere line
 */
int brook_config_file_read_line(int fd, char *buffer, int buffer_size) {

  char c;
  int i = 0;

  // We don't write in last character of buffer to add the bit \0 in end of string
  while (i < buffer_size - 1) {
    ssize_t b = read(fd, &c, 1);

    // Error reading from file descritor / file
    if (b == -1) {
      perror("read from config file");
      return BROOK_ERROR;
    }

    // Will try again
    if (b == 0) {
      continue;
    }

    // Ignore end of lines
    if (c == '\n') {
      break;
    }

    if (c == '\r') {
      continue;
    }

    buffer[i++] = c;
  }

  buffer[i] = '\0';

  return i;
}
