#ifndef BROOK_JOB_QUEUE_H
#define BROOK_JOB_QUEUE_H
#define BROOK_JOB_MAX 256

#include "beanstalkclient.h"

typedef struct {
  unsigned long id; // id beanstalkd
  unsigned int prio;
  unsigned int delay;
  unsigned int ttr;
  char tube[160];
  unsigned char *payload;
  size_t len;
  int state; // free, pending, in-flight, ...
} brook_job_t;

typedef struct {
  int fd;
  bsc *client;

  brook_job_t jobs[BROOK_JOB_MAX];
  int njobs;
} brook_job_queue_t;

int brook_job_queue_create(void);
void brook_job_queue_on_read(void *ctx);
void brook_job_queue_on_write(void *ctx);

#endif
