#ifndef IO_H
#define IO_H

#include "base.h"

#include "BW_protocol.h"
#include "channel.h"

//int dup2_and_close(int socket, int to);
int send_fd(int sock, channel_t *ch, int size);
int recv_fd(int sock, channel_t *ch, int size);

#endif
