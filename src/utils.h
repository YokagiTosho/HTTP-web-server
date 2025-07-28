#ifndef UTILS_H
#define UTILS_H

#include "base.h"

int sus_redir_stream(const char *log_path, int stream_fd);
char *sus_http_gmtime(time_t _time);


#endif
