#ifndef RESPONSE_H
#define RESPONSE_H

#include "base.h"

#include "http.h"
#include "compress.h"
#include "date.h"
#include "errors_html.h"

typedef struct {
	int http_version;
	int status_code;
	const char *verbose;

	int content_length;

	const char *connection;
	int content_type;
	char *content_language;
	char *location;
	char *set_cookie;
	const char *content_encoding; // if not NULL, then must be compressed with compresser it points to
	char *transfer_encoding; // if not NULL and points to string 'chunked', field chunked should be 1
	char *date;
	char *last_modified;
	const char *server;

	const char *body;

	unsigned chunked:1;
	unsigned supported_encodings:3;
	unsigned do_compress:1;

	int chunking_handle;
} response_t;

int sus_response_from_fd(int fd, response_t *response);
int sus_set_default_headers(response_t *response);

int sus_send_response(int fd, response_t *request);
int sus_send_response_error(int fd, int status);

void sus_fre_res(response_t *response);

//int sus_response_from_cgi(int fd, response_t *response);

#endif
