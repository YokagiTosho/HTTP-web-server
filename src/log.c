#include "log.h"

#define LOG_BUFSIZE 1024

static char *buf;

int log_error(int log_level, const char *fmt, const char *srcfile, const char *funcname, int line, ...)
{
	/* example: [ dd:mm:yyyy ] msg \ file.c, function, line \ */
	if (log_level == LEVEL_DEBUG) {
		return SUS_OK;
	}

	va_list ap;
	int ret;
	struct tm *t;
	time_t now = time(0);

	va_start(ap, line);

	if (!buf) {
		buf = malloc(LOG_BUFSIZE);
		if (!buf) {
			fprintf(stderr, 
					"Could not allocate memory for buf in log_error."
					"Logging error happened in function to log error\n");
			exit(1);
		}
	}

	memset(buf, 0, LOG_BUFSIZE);

	ret = vsnprintf(buf, LOG_BUFSIZE, fmt, ap);
	buf[ret] = '\0';

	t = localtime(&now);

	fprintf(
			stderr, 
			"[ %.2d-%.2d-%d, %.2d:%.2d:%.2d ]\t%s\t \\ \"%s\",    \"%s\",    %d \\\n", 
			t->tm_mday, t->tm_mon+1, t->tm_year+1900, 
			t->tm_hour, t->tm_min, t->tm_sec, 
			buf, srcfile, funcname, line);

	va_end(ap);
	return SUS_OK;
}
