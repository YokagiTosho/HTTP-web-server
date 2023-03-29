#include "cgi.h"

void execute_cgi(int fd, void *data)
{
	struct cgi_data *cgi_data = (struct cgi_data *) data;

#ifdef DEBUG
	printf("from run_cgi: %s\n", cgi_data->fs_path);
#endif

	exit(0);
}
