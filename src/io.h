#ifndef IO_H
#define IO_H

#include "base.h"

#include "channel.h"

int sus_send_fd(int sock, channel_t *ch, int size);
int sus_recv_fd(int sock, channel_t *ch, int size);

enum {
	WORKER_GET_CONNECTIONS = 0x1,
	WORKER_RECV_FD = 0x2,
};

#endif
