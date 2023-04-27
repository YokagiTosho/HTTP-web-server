#ifndef CGI_H
#define CGI_H

#include "base.h"

#include "request.h"
#include "process.h"
#include "response.h"

#include "filepath.h"

void sus_execute_cgi(int fd, void *data);
int sus_run_cgi(int fd, const request_t *request);

#endif
