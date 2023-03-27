#ifndef LOG_H
#define LOG_H

#include "base.h"


#define LEVEL_DEBUG 0
#define LEVEL_WARNING 1
#define LEVEL_PANIC 2

int log_error(
		int log_level,
		const char *fmt,
		const char *srcfile,
		const char *funcname,
		int line,
		...);


#define sus_log_error(log_level, msg, ...) \
	log_error(log_level, msg, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif
