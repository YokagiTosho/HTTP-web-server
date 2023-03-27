#ifndef RESPONSE_H
#define RESPONSE_H

#include "base.h"

#include "http.h"

typedef struct {
	int http_version;
	int status_code;
	const char *verbose;

#if 0
	size_t content_length;
	const char *connection;
	const char *content_type;
	const char *content_language;
	const char *location;
	const char *server;
	const char *set_cookie;
	const char *last_modified;
	char *content_encoding;
	char *date;
#endif
	int content_length;
	char *connection;
	char *content_type;
	char *content_language;
	char *location;
	char *set_cookie;
	char *last_modified;
	char *content_encoding;
	char *date;
	const char *server;

	char *body;

} response_t;

int send_response(int fd);
void fre_res(response_t *response);

#endif
