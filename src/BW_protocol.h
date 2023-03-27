#ifndef BW_PROTOCOL_H
#define BW_PROTOCOL_H

#include "base.h"

enum BWprotocol_numbers {
	WORKER_GET_CONNECTIONS = 0x1,
	WORKER_RECV_FD = 0x2,
};

char BW_read_bridgecode(int fd);
int BW_tobridge_0x1(int fd, int busy, int max);

#endif
