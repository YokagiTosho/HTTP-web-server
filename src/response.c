#include "response.h"

/* NOTE THIS IS TEST FUNCTION TO SEND TEST RESPONSE */
int send_response(int fd)
{
	size_t size;
	int wc;
	char test_answer[] = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Connection: keep-alive\r\n"
		"Content-Length: 29\r\n"
		"\r\n"
		"<h1>Test msg from server</h1>";

	size = sizeof(test_answer)-1;
	if ((wc = write(fd, test_answer, size)) != size) {
		sus_log_error(LEVEL_WARNING, "write(%d) != size(%d): %s", wc, size, strerror(errno));
		return SUS_ERROR;
	}
	return SUS_OK;
}

static int response_from_file(int fd, response_t *response)
{


	return SUS_OK;
}

static int response_from_cgi(int fd, response_t *response)
{


	return SUS_OK;
}

int response_from_fd(int fd, response_t *response)
{
	struct stat statbuf;
	if (fstat(fd, &statbuf) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"fstat()\": %s", strerror(errno));
		return SUS_ERROR;
	}

	if (S_ISSOCK(statbuf.st_mode)) {
		return response_from_cgi(fd, response);
	} else if (S_ISREG(statbuf.st_mode)) {
		return response_from_file(fd, response);
	} else {
		sus_log_error(LEVEL_PANIC, "Undefined result in \"response_from_fd()\"");
		return SUS_ERROR;
	}

	return SUS_OK;
}

void fre_res(response_t *response)
{


}
