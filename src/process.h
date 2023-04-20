#ifndef PROCESS_H
#define PROCESS_H

#include "base.h"

typedef struct {
	pid_t pid;
	int channel[2];
} process_t;

int sus_create_process(process_t *proc, void (*callback)(int fd, void *data), void *data);
int sus_wait_process(const process_t *process);
int sus_close_process(process_t *process);

#endif
