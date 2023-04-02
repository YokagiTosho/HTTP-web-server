#ifndef IO_H
#define IO_H

#include "base.h"

#include "BW_protocol.h"
#include "channel.h"

int sus_send_fd(int sock, channel_t *ch, int size);
int sus_recv_fd(int sock, channel_t *ch, int size);

#endif
