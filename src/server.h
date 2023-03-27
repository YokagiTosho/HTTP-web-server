#ifndef SERVER_H
#define SERVER_H

#include "base.h"

#include "config.h"

#if 0
typedef struct {
	int socket;
	int addr;
	short port;
	int max_connections;
	
	//struct pollfd *fds;
} http_server_t;
#endif

//http_server_t *mkserver();
int server_fd_init();

#endif
