#include "static.h"

int sus_run_static(int fd, const request_t *request)
{
	int static_file_fd;
	response_t response;
	const char *fs_path = sus_get_fspath();

	memset(&response, 0, sizeof(response_t));

	response.content_type = sus_get_content_type(fs_path);

	if (request->accept_encoding && strstr(request->accept_encoding, "gzip")) {
		response.supported_encodings |= GZIP;
	}

	sus_set_default_headers(&response);

	static_file_fd = open(fs_path, O_RDONLY);
	if (static_file_fd == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"open()\" %s: %s", fs_path, strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (sus_response_from_fd(static_file_fd, &response) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (sus_send_response(fd, &response) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (close(static_file_fd) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	sus_fre_res(&response);
	return SUS_OK;
}
