#include "response.h"

//#define BUFLEN 128 // NOTE reduced from 8192 to implement chunking and test it. Make this 8192 back again
//#define BUFLEN 8192 // NOTE reduced from 8192 to implement chunking and test it. Make this 8192 back again
#define BUFLEN 2048
static char *buf; // NOTE buf is global variable, because worker is single-threaded thus syncronised,
                  // no double access to the same memory

struct raw_response {
	char *bytes;
	int size;

	int stline_length;
	int headers_length;
	int content_length;
};

static int response_add_str(char *dest, int *offset, const char *fmt, ...)
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

int set_default_headers(response_t *response)
{
	response->server = "SUS/0.1";
	response->http_version = HTTP_VERSION1_1;
	response->date = http_gmtime(time(0));

	return SUS_OK;
}


static int response_set_raw_hdrs(struct raw_response *raw, const response_t *response)
{
	int size = 0;

	char *http_raw = raw->bytes;
	
	response_add_str(http_raw, &size, "%s %d %s\r\n",
			http_version_to_str(response->http_version),
			response->status_code,
			response->verbose);

	raw->stline_length = size;
	//fprintf(stdout, "raw->stline_length: %d\n", raw->stline_length);

	if (response->content_length > 0) {
		response_add_str(http_raw, &size, "Content-Length: %d\r\n", response->content_length);
	}
	if (response->connection) {
		response_add_str(http_raw, &size, "Connection: %s\r\n", response->connection);
	}
	if (response->content_type != SUS_ERROR) {
		response_add_str(http_raw, &size, "Content-Type: %s\r\n", content_type_to_str(response->content_type));
	}
	if (response->content_language) {
		response_add_str(http_raw, &size, "Content-Language: %s\r\n", response->content_language);
	}
	if (response->location) {
		response_add_str(http_raw, &size, "Location: %s\r\n", response->location);
	}
	if (response->set_cookie) {
		response_add_str(http_raw, &size, "Set-Cookie: %s\r\n", response->set_cookie);
	}
	if (response->content_encoding) {
		response_add_str(http_raw, &size, "Content-Encoding: %s\r\n", response->content_encoding);
	}
	if (response->transfer_encoding) {
		response_add_str(http_raw, &size, "Transfer-Encoding: %s\r\n", response->transfer_encoding);
	}
	if (response->date) {
		response_add_str(http_raw, &size, "Date: %s\r\n", response->date);
	}
	if (response->last_modified) {
		response_add_str(http_raw, &size, "Last-Modified: %s\r\n", response->last_modified);
	}
	if (response->server) {
		response_add_str(http_raw, &size, "Server: %s\r\n", response->server);
	}
	response_add_str(http_raw, &size, "\r\n"); // end of headers

	raw->headers_length = size - raw->stline_length;
	//fprintf(stdout, "raw->headers_length: %d\n", raw->headers_length);

	return size;
}

static int response_set_raw_body(struct raw_response *raw, const response_t *response)
{
	int offset = raw->stline_length + raw->headers_length;
	int size = response->content_length;

	if (response->body) {
		if (response->content_length > 0) {
			memcpy(raw->bytes+offset,
					response->body,
					size);
			raw->content_length = size;
			//fprintf(stdout, "raw->content_length: %d\n", raw->content_length);
		} else {
			sus_log_error(LEVEL_PANIC, "response->content_length <= 0 in \"response_to_raw()\"");
			return SUS_ERROR;
		}
	} else {
		sus_log_error(LEVEL_PANIC, "response->body is NULL in \"response_to_raw()\"");
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int response_to_raw(struct raw_response *raw, const response_t *response)
{
	/* Used to make raw bytes from response, to send over network */

	/* TODO after Memory Pool is implemented, allocate there, instead of malloc */
	raw->bytes = malloc(BUFLEN);
	if (!raw->bytes) {
		sus_log_error(LEVEL_PANIC, "Failed to alloc mem for raw->bytes");
		exit(0);
	}
	
	response_set_raw_hdrs(raw, response);

	if (!response->chunked) {
		response_set_raw_body(raw, response);
	}

	raw->size = raw->stline_length + raw->headers_length + raw->content_length;
	raw->bytes[raw->size] = 0;

	return SUS_OK;
}

static int send_chunked(int fd, const response_t *response)
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

static int send_headers(int fd, const struct raw_response *raw)
{
	int ret;
	int size = raw->stline_length+raw->headers_length;

	ret = write(fd, raw->bytes, size);
	if (ret != size || ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to write headers: %s", strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int send_body(int fd, const struct raw_response *raw)
{
	int ret;
	int offset = raw->stline_length + raw->headers_length;
	int size = raw->content_length;

	ret = write(fd, raw->bytes+offset, size);
	if (ret != size || ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to write body: %s", strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int response_compress(response_t *response)
{
	if (response->supported_encodings & GZIP) {
		if (compress_data((char *)response->body, &response->content_length, GZIP) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"compress_data()\"");
			return SUS_ERROR;
		}
	}
	return SUS_OK;
}

int send_response(int fd, response_t *response)
{
	struct raw_response raw = {
		.bytes          = NULL,
		.stline_length  = 0,
		.headers_length = 0,
		.content_length = 0,
	};

	/* TODO decide here: do I need to compress data and chunk it or not 
	 * If compress: compress here response body 
	 * If chunk: first send headers to User-Agent(fd),
	 *           then splitted body(it must be splitted first, obviously */
	if (response->do_compress) {
		if (response_compress(response) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"response_compress()\"");
			goto error;
		}
	}

	if (response_to_raw(&raw, response) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"response_to_raw()\"");
		goto error;
	}
#if 0
#ifdef DEBUG
	printf("After \"response_to_raw()\": %s", raw.bytes);
#endif
#endif
	if (send_headers(fd, &raw) == SUS_ERROR) {
		goto error;
	}

	if (response->chunked) {
		if (send_chunked(fd, response) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"send_chunked()\"");
			goto error;
		}
	} else {
		if (send_body(fd, &raw) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"send_body()\"");
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

int send_hardcoded_msg(int fd, const char *msg, int size)
{
	int wc;

	if ((wc = write(fd, msg, size)) != size) {
		sus_log_error(LEVEL_WARNING, "write(%d) != size(%d) for not_found_msg: %s", wc, size, strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int response_from_file(int fd, response_t *response, struct stat *statbuf)
{
	/* TODO read fd(file), make response */
	int ret, file_size;

	file_size = statbuf->st_size;
	if (file_size >= BUFLEN) {
		response->chunked = 1;
	}

	if (response->content_type != IMAGE_JPEG 
			&& response->content_type != IMAGE_PNG
			&& file_size > 1024
			&& !response->chunked)
	{
		response->do_compress = 1;
	}

	/* TODO: maybe, after I implement Pool Of Memory(if I will be able to), instead
	 * having separate 'buf' global variable, allocate memory in pool for response->body
	 * and write in it.
	 * And if an error occured, just label pool chunk as free, instead of mallocs()/frees() */
	response->status_code   = HTTP_OK;
	response->verbose       = set_verbose(response->status_code);
	response->last_modified = http_gmtime(statbuf->st_mtim.tv_sec);

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

static int response_from_cgi(int fd, response_t *response)
{
	//int ret, content_len;
	/* TODO read for CGI output(fd), parse it, decide to chunk or not to chunk, make response */
	return SUS_OK;
}

int response_from_fd(int fd, response_t *response)
{
	struct stat statbuf;

	if (fstat(fd, &statbuf) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"fstat()\": %s", strerror(errno));
		return SUS_ERROR;
	}

	if (!buf) {
		if ((buf = malloc(BUFLEN+1)) == NULL) {
			sus_log_error(LEVEL_PANIC, "Failed alloc mem for buf");
			exit(1);
		}
	}

	if (S_ISSOCK(statbuf.st_mode)) {
		return response_from_cgi(fd, response);
	} else if (S_ISREG(statbuf.st_mode)) {
		return response_from_file(fd, response, &statbuf);
	} else {
		sus_log_error(LEVEL_PANIC, "Undefined S_IS* in \"response_from_fd()\"");
		return SUS_ERROR;
	}
	return SUS_OK;
}

void fre_res(response_t *response)
{
#define FRE_NNUL(FIELD) \
	do { \
		if (response->FIELD) { \
			free(response->FIELD); \
			response->FIELD = NULL; \
		} \
	} while (0)

	FRE_NNUL(connection);
	FRE_NNUL(content_language);
	FRE_NNUL(location);
	FRE_NNUL(set_cookie);
	FRE_NNUL(last_modified);
	FRE_NNUL(content_encoding);
	FRE_NNUL(date);
	//FRE_NNUL(body);

	response->transfer_encoding = NULL;
	response->body = NULL;
	memset(buf, 0, BUFLEN);

	response->chunking_handle = -1;
}
