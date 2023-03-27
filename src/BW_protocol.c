#include "BW_protocol.h"

char BW_read_bridgecode(int fd)
{
	char code;
	if (read(fd, &code, sizeof(char)) != sizeof(char)) {
		return -1;
	}
	return code;
}

int BW_tobridge_0x1(int fd, int busy, int max)
{
	/* WORKER_GET_CONNECTIONS(BW_protocol.h) implementation */
	struct { int busy; int max; } response;
	response.busy = busy;
	response.max = max;

	int n = sizeof(response);
	if (write(fd, &response, n) != n) {
		return -1;
	}
	return 0;
}
