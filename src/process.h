#ifndef PROCESS_H
#define PROCESS_H

#include "base.h"

typedef struct {
	pid_t pid;
	int channel[2];
} process_t;

process_t create_process(void (*callback)(int fd));

#endif
