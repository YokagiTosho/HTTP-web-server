#include "request.h"

#define URI_MAXLEN 257
#define HTTP_VERSIONLEN 10
#define HTTP_METHODLEN 12
#define HDRLEN 256

static int sus_get_hdr_type(const char *hdrtype) 
{
	if (strcmp(hdrtype, "Accept-Language") == 0) {
		return hdr_accept_language;
	} else if (strcmp(hdrtype, "Accept-Encoding") == 0) {
		return hdr_accept_encoding;
	} else if (strcmp(hdrtype, "Accept") == 0) {
		return hdr_accept;
	} else if (strcmp(hdrtype, "Connection") == 0) {
		return hdr_connection;
	} else if (strcmp(hdrtype, "Cookie") == 0) {
		return hdr_cookie;
	} else if (strcmp(hdrtype, "Content-Length") == 0) {
		return hdr_content_length;
	} else if (strcmp(hdrtype, "Content-Type") == 0) {
		return hdr_content_type;
	} else if (strcmp(hdrtype, "Origin") == 0) {
		return hdr_origin;
	} else if (strcmp(hdrtype, "User-Agent") == 0) {
		return hdr_user_agent;
	} else if (strcmp(hdrtype, "Host") == 0) {
		return hdr_host;
	}
	return hdr_undefined;
}

static int sus_get_http_method(const char *method)
{
	if (strcmp(method, "GET") == 0) {
		return HTTP_GET;
	} else if (strcmp(method, "POST") == 0) {
		return HTTP_POST;
	} else if (strcmp(method, "OPTIONS") == 0) {
		return HTTP_OPTIONS;
	} else if (strcmp(method, "DELETE") == 0) {
		return HTTP_DELETE;
	} else if (strcmp(method, "PUT") == 0) {
		return HTTP_PUT;
	} else if (strcmp(method, "UPDATE") == 0) {
		return HTTP_UPDATE;
	}

	sus_log_error(LEVEL_PANIC, "Unsupported HTTP method: %s", method);
	sus_set_errno(HTTP_BAD_REQUEST);
	return SUS_ERROR;
}

static int sus_get_http_version(const char *http_version)
{
	if (!strcmp(http_version, "HTTP/1.1")) {
		return HTTP_VERSION1_1;
	}

	sus_log_error(LEVEL_PANIC, "HTTP version not supported: %s", http_version);
	sus_set_errno(HTTP_VERSION_NOT_SUPPORTED);
	return SUS_ERROR;
}

static int sus_end_of_headers(const char *s)
{
	return s[0] == '\r' && s[1] == '\n';
}

static int sus_get_next_header(char **req, char *hdr, int size)
{
	/* NOTE maybe return pointer to next chunk of request, instead of INC */
	int i;
	char hdr_type[64];
	char *s = *req;

	if (sus_end_of_headers(s)) {
		/* INC pointer by 2 to go through '\r\n' */
		*req += 2;
		return 0;
	}

	/* going through header key and checking it for errors */
	for (i = 0; s[i] != ':' && size; i++, size--) {
		hdr_type[i] = s[i];
	}
	if (size <= 0) {
		/* didn't find ':' */
		sus_log_error(LEVEL_PANIC, "size <= 0: Bad request");
		sus_set_errno(HTTP_BAD_REQUEST);
		return SUS_ERROR;
	}
	hdr_type[i] = '\0';

	/* skipping unwanted chars */
	if (s[i] == ':') {
		i++;
	} else {
		sus_set_errno(HTTP_BAD_REQUEST);
		return SUS_ERROR;
	}
	if (s[i] == ' ') {
		i++;
	} else {
		sus_set_errno(HTTP_BAD_REQUEST);
		return SUS_ERROR;
	}

	/* header value */
	for (; s[i] != '\r' && s[i+1] != '\n' && size; i++, size--) {
		*hdr = s[i];
		hdr++;
	}
	if (size <= 0) {
		sus_log_error(LEVEL_PANIC, "size <= 0: request too big");
		sus_set_errno(HTTP_URI_TOO_LONG);
		return SUS_ERROR;
	}
	*hdr = '\0';

	/* get rid of '\r\n' at the end of a header */
	*req += i+2;

	return sus_get_hdr_type(hdr_type);
}

static int sus_parse_request_version(char **req, char *version, int size)
{
	int i;
	char *r = *req;
	for (i = 0; r[i] != '\r' && r[i+1] != '\n' && size; i++, size--) {
		version[i] = r[i];
	}
	if (size <= 0) {
		sus_log_error(LEVEL_PANIC, "size <= 0: request too big");
		sus_set_errno(HTTP_URI_TOO_LONG);
		return SUS_ERROR;
	}
	version[i] = '\0';

	*req = r+i+2;

	return SUS_OK;
}

static int sus_parse_request_method(char **req, char *method, int size)
{
	int i;
	char *r = *req;
	for (i = 0; r[i] != ' ' && size; i++, size--) {
		method[i] = r[i];
	}
	if (size <= 0) {
		sus_log_error(LEVEL_PANIC, "size <= 0: request too big");
		sus_set_errno(HTTP_URI_TOO_LONG);
		return SUS_ERROR;
	}
	method[i] = '\0';

	*req = r+i+1;

	return SUS_OK;
}

static int sus_parse_request_uri(char **req, char *uri, int size)
{
	int i;
	char *r = *req;

	for (i = 0; r[i] != ' ' && size; i++, size--) {
		uri[i] = r[i];
	}

	if (size <= 0) {
		sus_log_error(LEVEL_PANIC, "size <= 0: request too big");
		sus_set_errno(HTTP_URI_TOO_LONG);
		return SUS_ERROR; /* NOTE URI too long */
	}

	uri[i] = '\0';

	*req = r+i+1;
	return 0;
}

static int sus_is_cgi(const char *uri)
{
	int i, len1, len2;
	const char *cgi_dir = sus_get_config_cgidir();

	len1 = strlen(cgi_dir);
	len2 = strlen(uri);

	if (len2 < len1) {
		return FALSE;
	}

	for (i = 0; i < len1; i++) {
		if (uri[i] != cgi_dir[i]) {
			return FALSE;
		}
	}

	return TRUE;
}

int sus_parse_request(char *rawreq, request_t *s_req)
{
	/* TODO validate that server is working with valid HTTP request.
	 * It can be easily broken with message like aaaaaa\r\n, no ':' found, thus infinite loop */
	int ret, g;
	char http_version[HTTP_VERSIONLEN], method[HTTP_METHODLEN], uri[URI_MAXLEN];
	char *req = rawreq;

	/* skip first N '\r\n' lines if exist */
	while (*req == '\r' && *(req+1) == '\n') {
		req += 2;
	}

	/* METHOD */
	ret = sus_parse_request_method(&req, method, HTTP_METHODLEN);
	if (ret == SUS_ERROR) {
		return SUS_ERROR;
	}

	s_req->method = sus_get_http_method(method);
	if (s_req->method == SUS_ERROR) {
		return SUS_ERROR;
	}

	/* URI */
	ret = sus_parse_request_uri(&req, uri, URI_MAXLEN);
	if (ret == SUS_ERROR) {
		return SUS_ERROR;
	}
	MLC_CPY(s_req->uri, uri);

	/* VERSION */
	ret = sus_parse_request_version(&req, http_version, HTTP_VERSIONLEN);
	if (ret == SUS_ERROR) {
		return SUS_ERROR;
	}

	s_req->http_version = sus_get_http_version(http_version);
	if (s_req->http_version == SUS_ERROR) {
		return SUS_ERROR;
	}

	/* HEADERS */
	g = 1;
	while (g) {
		char hdr_value[HDRLEN];
		ret = sus_get_next_header(&req, hdr_value, HDRLEN);
		switch (ret) {
			case SUS_ERROR:
				/* TODO error */
				sus_log_error(LEVEL_PANIC, "Failed \"get_next_header()\"");
				return SUS_ERROR;
				break;
			case 0:
				g = 0;
				break;
			case hdr_accept_language:
				MLC_CPY(s_req->accept_language, hdr_value);
				break;
			case hdr_accept_encoding:
				MLC_CPY(s_req->accept_encoding, hdr_value);
				break;
			case hdr_accept:
				MLC_CPY(s_req->accept, hdr_value);
				break;
			case hdr_connection:
				MLC_CPY(s_req->connection, hdr_value);
				break;
			case hdr_cookie:
				MLC_CPY(s_req->cookie, hdr_value);
				break;
			case hdr_content_length:
				errno = 0;
				CONVERT_TO_INT(s_req->content_length, hdr_value);
				break;
			case hdr_content_type:
				MLC_CPY(s_req->content_type, hdr_value);
				break;
			case hdr_origin:
				MLC_CPY(s_req->origin, hdr_value);
				break;
			case hdr_user_agent:
				MLC_CPY(s_req->user_agent, hdr_value);
				break;
			case hdr_host:
				MLC_CPY(s_req->host, hdr_value);
				break;
		}
	}

	if (s_req->content_length > 0) {
		MLC_CPY(s_req->request_body, req);
	}

	// NOTE extra fields
	if (!strchr(s_req->uri, '.')) {
		s_req->dir = 1;
	}
	if (strchr(s_req->uri, '?')) {
		s_req->args = 1;
	}
	if (sus_is_cgi(s_req->uri)) {
		s_req->cgi = 1;
	}

	return SUS_OK;
}

void sus_dump_request(const request_t *request)
{
#ifdef DEBUG
	fprintf(stdout,
			"uri: %s\n"
			"host: %s\n"
			"accept: %s\n"
			"accept_encoding: %s\n"
			"accept_language: %s\n"
			"connection: %s\n"
			"cookie: %s\n"
			"content_type: %s\n"
			"content_length: %d\n"
			"origin: %s\n"
			"user_agent: %s\n"
			"request_body: %s\n",
			request->uri,
			request->host,
			request->accept,
			request->accept_encoding,
			request->accept_language,
			request->connection,
			request->cookie,
			request->content_type,
			request->content_length,
			request->origin,
			request->user_agent,
			request->request_body);
#endif
}

int sus_log_access(const request_t *request, int sz)
{
#ifndef DEBUG
	time_t now = time(0);
	struct tm *t;
	t = localtime(&now);
	fprintf(
			stdout, 
			"[ %.2d-%.2d-%d, %.2d:%.2d:%.2d ]\t%s\t%d\n", 
			t->tm_mday, t->tm_mon+1, t->tm_year+1900, 
			t->tm_hour, t->tm_min, t->tm_sec, 
			request->uri, sz);
#endif
	return SUS_OK;
}

void sus_fre_req(request_t *request)
{
#define FRE_NNUL(FIELD) \
	do { \
		if (request->FIELD) { \
			free(request->FIELD); \
			request->FIELD = NULL; \
		} \
	} while (0)

	FRE_NNUL(uri);
	FRE_NNUL(host);
	FRE_NNUL(accept);
	FRE_NNUL(accept_encoding);
	FRE_NNUL(accept_language);
	FRE_NNUL(connection);
	FRE_NNUL(cookie);
	FRE_NNUL(content_type);
	FRE_NNUL(origin);
	FRE_NNUL(user_agent);
	FRE_NNUL(request_body);
}
