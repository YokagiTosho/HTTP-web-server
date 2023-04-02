#ifndef CGI_H
#define CGI_H

#include "base.h"

#include "request.h"

struct cgi_data {
	const char *fs_path;
	const request_t *request;
};

void sus_execute_cgi(int fd, void *data);

#endif
