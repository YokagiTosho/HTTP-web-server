#ifndef BRIDGE_H
#define BRIDGE_H

#include "base.h"

#include "server.h"
#include "worker.h"

#if 0
typedef struct bridge {
	pid_t pid;
	int fd;
} bridge_t;
#endif

int init_bridge(process_t *proc);
//void bridge_run(int main_fd, a_workers_t *workers);
//void bridge_run(int main_fd);
//void free_bridge(bridge_t *bridge);

#endif
