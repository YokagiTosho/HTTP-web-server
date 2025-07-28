#ifndef WORKER_H
#define WORKER_H

#include "base.h"

#include "server.h"
#include "io.h"
#include "process.h"

#include "request.h"
#include "response.h"

#include "cgi.h"
#include "static.h"

#define WORKER_BUFSIZE 4096

extern process_t workers[MAX_WORKERS];
extern int workerslen;

int sus_init_workers();

#endif
