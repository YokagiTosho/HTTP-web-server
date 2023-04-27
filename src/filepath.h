#ifndef FILEPATH_H
#define FILEPATH_H

#include "base.h"

#include "request.h"

#define MAX_PATHLEN 4096

int sus_create_fspath(const request_t *request);
const char *sus_get_fspath();

#endif
