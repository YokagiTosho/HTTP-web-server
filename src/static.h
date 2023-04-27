#ifndef STATIC_H
#define STATIC_H

#include "base.h"

#include "request.h"
#include "response.h"
#include "filepath.h"
#include "compress.h"

int sus_run_static(int fd, const request_t *request);

#endif
