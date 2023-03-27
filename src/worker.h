#ifndef WORKER_H
#define WORKER_H

#include "base.h"

#include "server.h"
#include "io.h"
#include "BW_protocol.h"
#include "process.h"

#include "request.h"
#include "response.h"

#define LISTEN_FD 0

#define WORKER_BUFSIZE 4096

extern process_t workers[MAX_WORKERS];
extern int workerslen;

int init_workers();

#endif
