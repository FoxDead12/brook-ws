#include "core/core.h"
#include <_regex.h>

/*
 * Method wo will read and store the config of server from file
 */
int brook_config(const char *config_file_path) {

  int file_fd;
  char buffer[1024];
  regex_t reegex;

  // Open destination file in read only mode
  file_fd = open(config_file_path, O_RDONLY, 0);

  if (file_fd == -1) {
    perror("file problem when open to read");
    return BROOK_ERROR;
  }

  while (1) {

    // Read a intere line of file
    int b = brook_config_file_read_line(file_fd, buffer, sizeof(buffer));
    if (b == 0) {
      continue;
    }

    printf("read %d: %.*s\n", b, b, buffer);
  }

  return BROOK_DONE;
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
