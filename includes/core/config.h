#ifndef _BROOK_CONFIG_H
#define _BROOK_CONFIG_H

int brook_config(const char *config_file_path);
int brook_config_file_read_line(int fd, char *buffer, int buffer_size);

#endif
