#include "request.h"

#define URI_MAXLEN 257
#define HTTP_VERSIONLEN 10
#define HTTP_METHODLEN 12
#define HDRLEN 256

static int sus_get_http_method(const char *method)
{
	if (!strcmp(method, "GET")) {
		return HTTP_GET;
	} else if (!strcmp(method, "POST")) {
		return HTTP_POST;
	} else if (!strcmp(method, "OPTIONS")) {
		return HTTP_OPTIONS;
	} else if (!strcmp(method, "DELETE")) {
		return HTTP_DELETE;
	} else if (!strcmp(method, "PUT")) {
		return HTTP_PUT;
	} else if (!strcmp(method, "UPDATE")) {
		return HTTP_UPDATE;
	} else if (!strcmp(method, "HEAD")) {
		return HTTP_HEAD;
	}

	sus_log_error(LEVEL_PANIC, "Unsupported HTTP method: %s", method);
	sus_set_errno(HTTP_BAD_REQUEST);
	return SUS_ERROR;
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

static int sus_parse_method(const char *rawreq, request_t *req)
{
	char *ch;
	int len;

	ch = strchr(rawreq, ' ');
	if (!ch) {
		sus_log_error(LEVEL_PANIC, "Failed to find \"' '\" after method in request");
		return SUS_ERROR;
	}

	len = (int)(ch-rawreq);
	if (!strncmp(rawreq, "GET", len)) {
		req->method = sus_get_http_method("GET");
	}
	else if (!strncmp(rawreq, "POST", len)) {
		req->method = sus_get_http_method("POST");
	}
	else if (!strncmp(rawreq, "OPTIONS", len)) {
		req->method = sus_get_http_method("OPTIONS");
	}
	else if (!strncmp(rawreq, "DELETE", len)) {
		req->method = sus_get_http_method("DELETE");
	}
	else if (!strncmp(rawreq, "PUT", len)) {
		req->method = sus_get_http_method("PUT");
	}
	else if (!strncmp(rawreq, "UPDATE", len)) {
		req->method = sus_get_http_method("UPDATE");
	}
	else if (!strncmp(rawreq, "HEAD", len)) {
		req->method = sus_get_http_method("HEAD");
	}
	else {
		sus_log_error(LEVEL_PANIC, "Undefined method in request");
		return SUS_ERROR;
	}

	return len+1;
}

static int sus_parse_uri(const char *rawreq, request_t *req)
{
	char *space_ch, *ques_ch;
	int uri_len, path_len, params_len;

	space_ch = strchr(rawreq, ' ');
	if (!space_ch) {
		sus_log_error(LEVEL_PANIC, "Failed to find \"' '\" after uri in request");
		return SUS_ERROR;
	}
	uri_len = (int)(space_ch-rawreq);

	
	ques_ch = memchr(rawreq, '?', uri_len);
	if (ques_ch) {
		req->args = 1;

		ques_ch++; // get rid of '?'

		params_len = (int)(space_ch-ques_ch);
		path_len = uri_len-params_len-1;

		MLCNCPY(req->path, rawreq, path_len);
		MLCNCPY(req->params, rawreq+path_len+1, params_len);
		MLCNCPY(req->uri, rawreq, uri_len);
	} else {
		path_len = uri_len;

		MLCNCPY(req->path, rawreq, path_len);
		MLCNCPY(req->uri, rawreq, uri_len);
	}

	if (!strchr(req->path, '.')) {
		req->dir = 1;
	}
	if (sus_is_cgi(req->path)) {
		req->cgi = 1;
	}

#if 0
		printf("req->url %s\n", req->path);
		printf("req->params %s\n", req->params);
		printf("req->uri %s\n", req->uri);
#endif

	return (uri_len+1); // +1 to get rid of space
}

static int sus_parse_version(const char *rawreq, request_t *req)
{
	int len;

	if (strstr(rawreq, "HTTP/0.9")) {
		return SUS_ERROR;
	} else if (strstr(rawreq, "HTTP/1.0")) {
		return SUS_ERROR;
	} else if (strstr(rawreq, "HTTP/1.1")) {
		req->method = HTTP_VERSION1_1;
		len = strlen("HTTP/1.1");
	}

	return (len+2);
}

static int sus_parse_next_header(const char *rawreq, request_t *req)
{
	char *colon, *header_end;
	int headerlen, valuelen, value_offset;

	if (*rawreq == '\r' && *(rawreq+1) == '\n') {
		return 0;
	}

	colon = strchr(rawreq, ':');
	if (!colon) {
		sus_log_error(LEVEL_PANIC, "Failed to find colon in header");
		return SUS_ERROR;
	}

	headerlen = (int)(colon-rawreq);

	header_end = strstr(rawreq, "\r\n");
	if (!header_end) {
		sus_log_error(LEVEL_PANIC, "Failed to find end of header");
		return SUS_ERROR;
	}
	valuelen = (int)(header_end-colon)-2; // -2 because of \r\n at the end

	value_offset = headerlen+2; // +2 to skip ':' and ' '

#define MLCCPYHDR(hdr) do { MLCNCPY(req->hdr, rawreq+value_offset, valuelen); } while(0)
	if (!strncmp(rawreq, "Accept-Language", 15)) {
		MLCCPYHDR(accept_language);
	} else if (!strncmp(rawreq, "Accept-Encoding", 15)) {
		MLCCPYHDR(accept_encoding);
	} else if (!strncmp(rawreq, "Accept", 6)) {
		MLCCPYHDR(accept);
	} else if (!strncmp(rawreq, "Connection", 10)) {
		MLCCPYHDR(connection);
	} else if (!strncmp(rawreq, "Cookie", 6)) {
		MLCCPYHDR(cookie);
	} else if (!strncmp(rawreq, "Content-Length", 14)) {
		req->content_length = strtol(rawreq+value_offset, NULL, 10);
		if ((req->content_length == LONG_MIN 
					|| req->content_length == LONG_MAX) && errno == ERANGE) {
			return SUS_ERROR;
		}
	} else if (!strncmp(rawreq, "Content-Type", 12)) {
		MLCCPYHDR(content_type);
	} else if (!strncmp(rawreq, "Origin", 6)) {
		MLCCPYHDR(origin);
	} else if (!strncmp(rawreq, "User-Agent", 10)) {
		MLCCPYHDR(user_agent);
	} else if (!strncmp(rawreq, "Host", 4)) {
		MLCCPYHDR(host);
	}

	return (int)(header_end-rawreq)+2;
}

int sus_parse_request(char *rawreq, request_t *s_req)
{
	int ret;
	char *req = rawreq;

	/* skip first N '\r\n' lines if exist */
	while (*req == '\r' && *(req+1) == '\n') {
		req += 2;
	}

	ret = sus_parse_method(req, s_req);
	if (ret == SUS_ERROR) {
		return SUS_ERROR;
	}
	req += ret;

	ret = sus_parse_uri(req, s_req);
	if (ret == SUS_ERROR) {
		return SUS_ERROR;
	}
	req += ret;

	ret = sus_parse_version(req, s_req);
	if (ret == SUS_ERROR) {
		return SUS_ERROR;
	}
	req += ret;

	if (*req == 0) {
		// no headers: error?
		sus_log_error(LEVEL_PANIC, "Nothing follows after starting line in request");
		return SUS_ERROR;
	}

	/* HEADERS */
	for ( ;; ) {
		ret = sus_parse_next_header(req, s_req);
		if (ret == SUS_ERROR) {
			return SUS_ERROR;
		} else if (ret == 0) {
			break;
		}

		req += ret;

		if (*req == 0) {
			break;
		}
	}

	if (s_req->content_length > 0) {
		MLC_CPY(s_req->request_body, req);
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
