#ifndef BASE_H
#define BASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <sys/stat.h>

#include "log.h" // NOTE included here to not include this file in every header

#define PARENT_FD 1
#define CHILD_FD  0

#define INVALID_SOCKET -1
#define SOCK_ERR       -1
#define INVALID_PID    -1

#define SERVER_SOCKET     0
#define CONNECTIONS_INDEX 1

#define SUS_DISCONNECTED 0
#define SUS_OK           0
#define SUS_ERROR       -1

#define MAX_WORKERS     16
#define MAX_CONNECTIONS 128
#define REQUEST_BUFSIZE 4096

#define DEBUG // for printf

#define CLOSE_INOUT_FD() \
	do { \
		close(STDIN_FILENO); \
		close(STDOUT_FILENO); \
	} while (0)

#endif
