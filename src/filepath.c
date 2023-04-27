#include "filepath.h"

static char path[MAX_PATHLEN];

int sus_create_fspath(const request_t *request)
{
	int ret;

	const char *basedir = sus_get_config_basedir();

	if (request->dir && !request->cgi) {
		ret = snprintf(path, MAX_PATHLEN, "%s/%s/%s", basedir, request->path, sus_get_config_default_html());
	} else if (request->dir && request->cgi){
		ret = snprintf(path, MAX_PATHLEN, "%s/%s/%s", basedir, request->path, sus_get_config_default_cgi());
	} else {
		ret = snprintf(path, MAX_PATHLEN, "%s/%s", basedir, request->path);
	}

	if (ret < 0 || ret >= MAX_PATHLEN) {
		sus_log_error(LEVEL_PANIC, "snprintf failed, ret val: %d: %s", ret, strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

const char *sus_get_fspath()
{
	if (path[0] != 0) {
		return path;
	}
	return NULL;
}
