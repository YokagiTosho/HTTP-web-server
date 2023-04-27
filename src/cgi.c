#include "cgi.h"

#define USE_setenv

void sus_execute_cgi(int fd, void *data)
{
	char str_int[32];
	const char *cgi_path;

	const request_t *request = (request_t*)data;

	cgi_path = sus_get_fspath();
	printf("cgi_path: %s\n", cgi_path);

	sprintf(str_int, "%d", request->content_length);

#if !defined (USE_setenv)

#ifdef DEBUG
	fprintf(stdout, "!defined (USE_setenv)\n");
#endif

	const char *envs[30];
	char *str;
	int i = 0, size;

#define SET_ENV(KEY, VAL, PRED) \
	do { \
		if (PRED) { \
			size = strlen(KEY)+strlen(VAL)+2; \
			str = malloc(size); \
			if (!str) { sus_log_error(LEVEL_PANIC, "Failed to alloc mem for str"); exit(1); } \
			snprintf(str, size, "%s=%s", KEY, VAL); \
			envs[i++] = str; \
			str = NULL; \
		} \
	} while (0)
	SET_ENV("HTTP_ACCEPT", request->accept, request->accept);
	SET_ENV("HTTP_ACCEPT_ENCODING", request->accept_encoding, request->accept_encoding);
	SET_ENV("HTTP_ACCEPT_LANGUAGE", request->accept_language, request->accept_language);
	SET_ENV("HTTP_CONNECTION", request->connection, request->connection);
	SET_ENV("HTTP_COOKIE", request->cookie, request->cookie);
	SET_ENV("HTTP_HOST", request->host, request->host);
	SET_ENV("HTTP_USER_AGENT", request->user_agent, request->user_agent);
	SET_ENV("DOCUMENT_URI", request->uri, request->uri);
	SET_ENV("QUERY_STRING", strchr(request->uri, '?')+1, request->args);
	SET_ENV("CONTENT_TYPE", request->content_type, request->content_type);
	SET_ENV("CONTENT_LENGTH", str_int, request->content_length>0);
	envs[i] = NULL;
#else
#define SET_ENV(KEY, VALUE, PREDICATE) \
	do { if (PREDICATE) setenv(KEY, VALUE, 0); } while (0)
	SET_ENV("HTTP_ACCEPT", request->accept, request->accept);
	SET_ENV("HTTP_ACCEPT_ENCODING", request->accept_encoding, request->accept_encoding);
	SET_ENV("HTTP_ACCEPT_LANGUAGE", request->accept_language, request->accept_language);
	SET_ENV("HTTP_CONNECTION", request->connection, request->connection);
	SET_ENV("HTTP_COOKIE", request->cookie, request->cookie);
	SET_ENV("HTTP_HOST", request->host, request->host);
	SET_ENV("HTTP_USER_AGENT", request->user_agent, request->user_agent);
	SET_ENV("DOCUMENT_URI", request->uri, request->uri);
	SET_ENV("QUERY_STRING", request->params, request->args);
	SET_ENV("CONTENT_TYPE", request->content_type, request->content_type);
	SET_ENV("CONTENT_LENGTH", str_int, request->content_length>0);
#endif

	if (dup2(fd, STDIN_FILENO) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"dup2(STDIN)\": %s", strerror(errno));
		exit(1);
	}
	if (dup2(fd, STDOUT_FILENO) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"dup2(STDOUT)\": %s", strerror(errno));
		exit(1);
	}
#if 0
	if (dup2(fd, STDERR_FILENO) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"dup2(STDERR)\": %s", strerror(errno));
		exit(1);
	}
#endif
	close(fd);

#if !defined (USE_setenv)
	execle(cgi_path, cgi_path, NULL, envs);
	sus_log_error(LEVEL_PANIC, "Failed \"execl()\": %s", strerror(errno));
	exit(1);
#else
	execl(cgi_path, cgi_path, NULL);
	sus_log_error(LEVEL_PANIC, "Failed \"execl()\": %s", strerror(errno));
	exit(1);
#endif
}

int sus_run_cgi(int fd, const request_t *request)
{
	/* Runs CGI script, parses its output, makes response and sends it to fd */
	process_t cgi_process;
	response_t response;
	int wstatus, size, ret;

	memset(&response, 0, sizeof(response_t));

	cgi_process.pid = INVALID_PID;
	cgi_process.channel[0] = INVALID_SOCKET;
	cgi_process.channel[1] = INVALID_SOCKET;

	sus_set_default_headers(&response);

	if (sus_create_process(&cgi_process, sus_execute_cgi, (void*)request) == SUS_ERROR) {
		return SUS_ERROR;
	}

	size = request->content_length;
	if (size > 0) {
		/* NOTE write POST data to CGI script */
		if (write(cgi_process.channel[0], request->request_body, size) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"write()\": %s", strerror(errno));
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
		}
	}

	if ((wstatus = sus_wait_process(&cgi_process)) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (WIFEXITED(wstatus)) {
		ret = WEXITSTATUS(wstatus);
		if (ret != 0) {
			sus_log_error(LEVEL_PANIC, "CGI script returned with %d", ret);
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
		}
	} else if (WIFSIGNALED(wstatus)) {
		sus_log_error(LEVEL_PANIC, "CGI script was killed by signal: %d", WTERMSIG(wstatus));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	} else if (WIFSTOPPED(wstatus)) {
		sus_log_error(LEVEL_PANIC, "CGI script was stopped by signal: %d", WSTOPSIG(wstatus));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (sus_response_from_fd(cgi_process.channel[0], &response) == SUS_ERROR) {
		goto error;
	}

	if (sus_send_response(fd, &response) == SUS_ERROR) {
		goto error;
	}

	sus_close_process(&cgi_process);
	sus_fre_res(&response);
	return SUS_OK;
error:
	sus_close_process(&cgi_process);
	sus_fre_res(&response);
	return SUS_ERROR;
}


