#ifndef BROOK_GLOBALS_H
#define BROOK_GLOBALS_H

#include "core/config.h"
#include "core/logger.h"
#include "core/process.h"

// Global state of each process, defined in brook.c
extern brook_config_node_t *conf; // root of the config tree
extern brook_logger_t logger;
extern brook_process_t process;

#endif
