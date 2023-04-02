#include "cgi.h"


void sus_execute_cgi(int fd, void *data)
{
	struct cgi_data *cgi_data = (struct cgi_data *) data;

#ifdef DEBUG
	fprintf(stderr, "from run_cgi: %s\n", cgi_data->fs_path);
#endif

	/* NOTE use setnev, instead making array of ENV variables */
	/* TODO prepare ENV from request */

	if (execl(cgi_data->fs_path, cgi_data->fs_path, (char *) NULL) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"execl()\": %s", strerror(errno));
		exit(1);
	}
	exit(1);
}
