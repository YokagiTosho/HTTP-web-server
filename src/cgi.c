#include "cgi.h"

//#define USE_setenv

void sus_execute_cgi(int fd, void *data)
{
	char str_int[32];

	struct cgi_data *cgi_data = (struct cgi_data *) data;
	const request_t *request = cgi_data->request;

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
	SET_ENV("QUERY_STRING", strchr(request->uri, '?')+1, request->args);
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
	execle(cgi_data->fs_path, cgi_data->fs_path, NULL, envs);
	sus_log_error(LEVEL_PANIC, "Failed \"execl()\": %s", strerror(errno));
	exit(1);
#else
	execl(cgi_data->fs_path, cgi_data->fs_path, NULL);
	sus_log_error(LEVEL_PANIC, "Failed \"execl()\": %s", strerror(errno));
	exit(1);
#endif
}
