#ifndef RESPONSE_H
#define RESPONSE_H

#include "base.h"

#include "http.h"

typedef struct {
	int http_version;
	int status_code;
	const char *verbose;

	int content_length;
	char *connection;
	const char *content_type;
	char *content_language;
	char *location;
	char *set_cookie;
	char *content_encoding; // if not NULL, then must be compressed with compresser it points to
	char *transfer_encoding; // if not NULL and points to string 'chunked', field chunked should be 1
	char *date;
	char *last_modified;
	const char *server;

	const char *body;

	unsigned chunked:1;
	int chunking_handle;
} response_t;

int response_from_fd(int fd, response_t *response);
int set_default_headers(response_t *response);

int send_hardcoded_msg(int fd, const char *msg, int size);
int send_response(int fd, response_t *request);

void fre_res(response_t *response);

#endif
