#include "response.h"

#define BUFLEN 8192
static char *buf; // NOTE buf is global variable, because worker is single-threaded thus syncronised,
                  // no double access to the same memory

struct raw_response {
	char *bytes;
	int size;

	int stline_length;
	int headers_length;
	int content_length;
};

static int sus_response_set_status(response_t *response, int status_code)
{
	response->status_code = status_code;
	response->verbose = sus_set_verbose(response->status_code);

	return SUS_OK;
}

static int sus_response_add_str(char *dest, int *offset, const char *fmt, ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);

	ret = vsprintf(dest + *offset, fmt, va);
	if (ret < 0) {
		sus_log_error(LEVEL_PANIC, "Failed \"vsprintf()\"");
		return SUS_ERROR;
	}

	*offset += ret;

	va_end(va);
	return SUS_OK;
}

int sus_set_default_headers(response_t *response)
{
	response->server = "SUS/0.1";
	response->http_version = HTTP_VERSION1_1;
	//response->connection = "keep-alive";
	response->date = sus_http_gmtime(time(0));

	return SUS_OK;
}


static int sus_response_set_raw_hdrs(struct raw_response *raw, const response_t *response)
{
	int size = 0;

	char *http_raw = raw->bytes;
	
	sus_response_add_str(http_raw, &size, "%s %d %s\r\n",
			sus_http_version_to_str(response->http_version),
			response->status_code,
			response->verbose);

	raw->stline_length = size;
	//fprintf(stdout, "raw->stline_length: %d\n", raw->stline_length);

	if (response->content_length > 0) {
		sus_response_add_str(http_raw, &size, "Content-Length: %d\r\n", response->content_length);
	}
	if (response->connection) {
		sus_response_add_str(http_raw, &size, "Connection: %s\r\n", response->connection);
	}
	if (response->content_type != SUS_ERROR) {
		sus_response_add_str(http_raw, &size, "Content-Type: %s\r\n", sus_content_type_to_str(response->content_type));
	}
	if (response->content_language) {
		sus_response_add_str(http_raw, &size, "Content-Language: %s\r\n", response->content_language);
	}
	if (response->location) {
		sus_response_add_str(http_raw, &size, "Location: %s\r\n", response->location);
	}
	if (response->set_cookie) {
		sus_response_add_str(http_raw, &size, "Set-Cookie: %s\r\n", response->set_cookie);
	}
	if (response->content_encoding) {
		sus_response_add_str(http_raw, &size, "Content-Encoding: %s\r\n", response->content_encoding);
	}
	if (response->transfer_encoding) {
		sus_response_add_str(http_raw, &size, "Transfer-Encoding: %s\r\n", response->transfer_encoding);
	}
	if (response->date) {
		sus_response_add_str(http_raw, &size, "Date: %s\r\n", response->date);
	}
	if (response->last_modified) {
		sus_response_add_str(http_raw, &size, "Last-Modified: %s\r\n", response->last_modified);
	}
	if (response->server) {
		sus_response_add_str(http_raw, &size, "Server: %s\r\n", response->server);
	}
	sus_response_add_str(http_raw, &size, "\r\n"); // end of headers

	raw->headers_length = size - raw->stline_length;
	//fprintf(stdout, "raw->headers_length: %d\n", raw->headers_length);

	return size;
}

static int sus_response_set_raw_body(struct raw_response *raw, const response_t *response)
{
	int offset = raw->stline_length + raw->headers_length;
	int size = response->content_length;

	if (response->body) {
		if (size > 0) {
			memcpy(raw->bytes+offset, response->body, size);
			raw->content_length = size;
			//fprintf(stdout, "raw->content_length: %d\n", raw->content_length);
		} else {
			sus_log_error(LEVEL_PANIC, "response->content_length <= 0 in \"response_to_raw()\"");
			sus_errno = HTTP_INTERNAL_SERVER_ERROR;
			return SUS_ERROR;
		}
	} else {
		sus_log_error(LEVEL_PANIC, "response->body is NULL in \"response_to_raw()\"");
		sus_errno = HTTP_INTERNAL_SERVER_ERROR;
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int sus_response_to_raw(struct raw_response *raw, const response_t *response)
{
	/* Used to make raw bytes from response to send over network */

	raw->bytes = malloc(BUFLEN);
	if (!raw->bytes) {
		sus_log_error(LEVEL_PANIC, "Failed to alloc mem for raw->bytes");
		exit(1);
	}
	
	sus_response_set_raw_hdrs(raw, response);

	if (!response->chunked) {
		sus_response_set_raw_body(raw, response);
	}

	raw->size = raw->stline_length + raw->headers_length + raw->content_length;
	raw->bytes[raw->size] = 0;

	return SUS_OK;
}

static int sus_send_chunked(int fd, const response_t *response)
{
	// TODO error check
	int ret;

	for ( ;; ) {
		ret = read(response->chunking_handle, buf, BUFLEN);
		if (ret == 0) {
			break;
		}
		buf[ret] = '\0';

		dprintf(fd, "%x\r\n", ret);
		// NOTE not using dprintf, because dprintf works only with null-terminated strings, 
		// while, for example, image, can have 0s anywhere
		write(fd, buf, ret);
		dprintf(fd, "\r\n");
	}
	dprintf(fd, "0\r\n\r\n");

	return SUS_OK;
}

static int sus_send_headers(int fd, const struct raw_response *raw)
{
	int ret;
	int size = raw->stline_length+raw->headers_length;

	ret = write(fd, raw->bytes, size);
	if (ret != size || ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to write headers: %s", strerror(errno));
		sus_errno = HTTP_INTERNAL_SERVER_ERROR;
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int sus_send_body(int fd, const struct raw_response *raw)
{
	int ret;
	int offset = raw->stline_length + raw->headers_length;
	int size = raw->content_length;

	ret = write(fd, raw->bytes+offset, size);
	if (ret != size || ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to write body: %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int sus_response_compress(response_t *response)
{
	if (response->supported_encodings & GZIP) {
		if (sus_compress_data((char *)response->body, &response->content_length, GZIP) == SUS_ERROR) {
			return SUS_ERROR;
		}
		response->content_encoding = "gzip";
	}
	return SUS_OK;
}

int sus_send_response(int fd, response_t *response)
{
	struct raw_response raw = {
		.bytes          = NULL,
		.stline_length  = 0,
		.headers_length = 0,
		.content_length = 0,
	};

	if (response->do_compress) {
		if (sus_response_compress(response) == SUS_ERROR) {
			goto error;
		}
	}

	if (sus_response_to_raw(&raw, response) == SUS_ERROR) {
		goto error;
	}
#if 0
#ifdef DEBUG
	fprintf(stdout, "After \"response_to_raw()\": %s", raw.bytes);
#endif
#endif
	if (sus_send_headers(fd, &raw) == SUS_ERROR) {
		goto error;
	}
	if (response->chunked) {
		if (sus_send_chunked(fd, response) == SUS_ERROR) {
			goto error;
		}
	} else {
		if (sus_send_body(fd, &raw) == SUS_ERROR) {
			goto error;
		}
	}

	if (raw.bytes) {
		free(raw.bytes);
		raw.bytes = NULL;
	}
	return SUS_OK;
error:
	if (raw.bytes) {
		free(raw.bytes);
		raw.bytes = NULL;
	}
	return SUS_ERROR;
}

int sus_send_response_error(int fd, int status)
{
	response_t response;
	int size;

	memset(&response, 0, sizeof(response_t));

	response.content_type = TEXT_HTML;

	sus_set_default_headers(&response);

	sus_response_set_status(&response, status);

	response.body = sus_get_html_error(status);
	if (!response.body) {
		return SUS_ERROR;
	}

	size = strlen(response.body);
	if (size > SIZE_TO_COMPRESS) {
		response.do_compress = 1;
	}

	response.content_length = size;

	if (sus_send_response(fd, &response) == SUS_ERROR) {
		return SUS_ERROR;
	}

	sus_fre_res(&response);
	return SUS_OK;
}

static int sus_response_from_file(int fd, response_t *response, struct stat *statbuf)
{
	int ret, file_size;

	file_size = statbuf->st_size;
	if (file_size >= BUFLEN) {
		response->chunked = 1;
	}

	if (response->content_type != IMAGE_JPEG 
			&& response->content_type != IMAGE_PNG
			&& file_size > SIZE_TO_COMPRESS
			&& !response->chunked)
	{
		response->do_compress = 1;
	}

	sus_response_set_status(response, HTTP_OK);
	response->last_modified = sus_http_gmtime(statbuf->st_mtim.tv_sec);

	if (!response->chunked) {
		ret = read(fd, buf, BUFLEN);
		switch (ret) {
			case -1:
			case  0:
				sus_log_error(LEVEL_PANIC, "Failed \"read()\": %s", strerror(errno));
				return SUS_ERROR;
		}
		buf[ret] = '\0';
		response->content_length = ret;
		response->body = buf; // NOTE point to the same memory as buf points to
	} else {
		response->chunking_handle = fd;
		response->transfer_encoding = "chunked";
		response->content_length = 0;
		response->body = NULL;
	}

	return SUS_OK;
}

static int sus_response_from_cgi(int fd, response_t *response)
{
	//int ret, content_len;
	/* TODO read for CGI output(fd), parse it, decide to chunk or not to chunk, make response */
	return SUS_OK;
}

int sus_response_from_fd(int fd, response_t *response)
{
	struct stat statbuf;

	if (fstat(fd, &statbuf) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"fstat()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (!buf) {
		if ((buf = malloc(BUFLEN+1)) == NULL) {
			sus_log_error(LEVEL_PANIC, "Failed alloc mem for buf");
			exit(1);
		}
	}

	if (S_ISSOCK(statbuf.st_mode)) {
		return sus_response_from_cgi(fd, response);
	} else if (S_ISREG(statbuf.st_mode)) {
		return sus_response_from_file(fd, response, &statbuf);
	} else {
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		sus_log_error(LEVEL_PANIC, "Undefined S_IS* in \"response_from_fd()\"");
		return SUS_ERROR;
	}
	return SUS_OK;
}

void sus_fre_res(response_t *response)
{
#define FRE_NNUL(FIELD) \
	do { \
		if (response->FIELD) { \
			free(response->FIELD); \
			response->FIELD = NULL; \
		} \
	} while (0)

	FRE_NNUL(content_language);
	FRE_NNUL(location);
	FRE_NNUL(set_cookie);
	FRE_NNUL(last_modified);
	FRE_NNUL(date);
	//FRE_NNUL(body);

	response->content_encoding = NULL;
	response->transfer_encoding = NULL;
	response->server = NULL;
	response->verbose = NULL;
	response->body = NULL;
	response->connection = NULL;

	//memset(buf, 0, BUFLEN);
	response->chunking_handle = -1;
}
