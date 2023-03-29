#ifndef CGI_H
#define CGI_H

#include "base.h"

struct cgi_data {
	const char *fs_path;
	char **env;
	int envlen;
};

void execute_cgi(int fd, void *data);

#endif
