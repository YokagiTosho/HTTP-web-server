#ifndef REQUEST_H
#define REQUEST_H

#include "base.h"

#include "http.h"
#include "config.h"

enum http_request_headers {
	hdr_accept_language = 1,
	hdr_accept_encoding,
	hdr_accept,
	hdr_connection,
	hdr_cookie,
	hdr_content_length,
	hdr_content_type,
	hdr_origin,
	hdr_user_agent,
	hdr_host,

	hdr_undefined,
};

typedef struct http_request {
	int method;
	int http_version;
	char *uri;

	char *host;
	char *accept;
	char *accept_encoding;
	char *accept_language;
	char *connection;
	char *cookie;
	char *content_type;
	char *origin;
	char *user_agent;

	int content_length;
	char *request_body;

	unsigned cgi:1;
	unsigned dir:1;
	unsigned args:1;

	//unsigned headers_only:1;
	//unsigned keepalive:1;
	//unsigned allowed_methods_only:1;
} request_t;

int sus_parse_request(char *req, request_t *s_req);
void sus_dump_request(const request_t *request);
int sus_log_access(const request_t *request, int sz);
void sus_fre_req(request_t *request);

#endif
