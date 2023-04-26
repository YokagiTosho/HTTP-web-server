#include "worker.h"

process_t workers[MAX_WORKERS]; // extern
int workerslen;                 // extern

static struct pollfd fds[MAX_CONNECTIONS];
static int nfds;

static int SIGINT_RECV;
static int SIGHUP_RECV;
static int SIGCHLD_RECV;

static void sus_set_signal(int sig)
{
	switch (sig) {
		case SIGINT:
			SIGINT_RECV = 1;
			break;
		case SIGHUP:
			SIGHUP_RECV = 1;
			break;
		case SIGCHLD:
			SIGCHLD_RECV = 1;
			break;
		default:
			break;
	}
}

static int sus_query_disconnect(int *socket)
{
	int s = *socket;

	if (s == INVALID_SOCKET) {
		sus_log_error(LEVEL_PANIC, "In \"sus_query_disconnect\" received \"INVALID_SOCKET\": %d", s);
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (close(s) == SOCK_ERR) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	*socket = INVALID_SOCKET;
	return SUS_OK;
}

static int sus_addfd_fds(int fd)
{
	if (nfds == MAX_CONNECTIONS) {
		sus_log_error(LEVEL_PANIC, "Reached maximum(%d) available sockets", MAX_CONNECTIONS);
		sus_set_errno(HTTP_SERVICE_UNAVAILABLE);
		return SUS_ERROR;
	}

	fds[nfds].fd = fd;
	fds[nfds].events = POLLIN;
	nfds++;

	return SUS_OK;
}

static void sus_align_fds()
{
	int i, j, n = nfds;

	// shift the rest
	for (i = 0; i < n; i++) {
		if (fds[i].fd == INVALID_SOCKET) {
			for (j = i; j < n; j++) {
				fds[j].fd = fds[j+1].fd;
			}
			n--;
			i--; // TODO maybe remove this line
			nfds--;
		}
	}
}

static int sus_process_bridge_data(int fd)
{
	int rc;
	channel_t ch;

	rc = sus_recv_fd(fd, &ch, sizeof(channel_t));
	if (rc == SUS_ERROR || rc == SUS_DISCONNECTED) {
		return rc;
	}

	if (ch.cmd == WORKER_RECV_FD) {
		if (sus_addfd_fds(ch.socket) == SUS_ERROR) {
			sus_send_response_error(fd, sus_errno);
			close(ch.socket);
			//return SUS_ERROR;
		}
	}
	return SUS_OK;
}

static int sus_read_clientfd(int fd, char *buf, int size)
{
	int rc;

	rc = read(fd, buf, size);
	switch (rc) {
		case -1:
			return SUS_ERROR;
		case 0:
			return SUS_DISCONNECTED;
	}
	buf[rc] = '\0';

	return rc;
}

static int sus_file_exists(const char *filepath)
{
	return access(filepath, F_OK | R_OK) != -1;
}

static int sus_create_fspath(char *fs_path, const request_t *request)
{
	int s, basedir_len;
	char *qptr;
	const char *basedir = sus_get_config_basedir();
	basedir_len = strlen(basedir);

	strncpy(fs_path, basedir, basedir_len);

	if (request->args) {
		qptr = strchr(request->uri, '?');
		if (!qptr) { 
			/* should not ever happen */
			sus_log_error(LEVEL_PANIC, "p == NULL should not never happen");
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
		}
		s = (int)(qptr - request->uri);
		strncat(fs_path, request->uri, s);
		//fprintf(stdout, "fs_path in request->args: %s\n", fs_path);
	} else {
		strcat(fs_path, request->uri);
	}

	if (request->dir && !request->cgi) {
		strcat(fs_path, sus_get_config_default_html());
	} else if (request->dir && request->cgi){
		strcat(fs_path, sus_get_config_default_cgi());
	}

	return SUS_OK;
}

static int sus_run_cgi(int fd, const request_t *request, const char *fs_path)
{
	/* Runs CGI script, parses its output, makes response and sends it to fd */
	process_t cgi_process;
	response_t response;
	struct cgi_data data;
	int wstatus, size, ret;

	memset(&response, 0, sizeof(response_t));

	cgi_process.pid = INVALID_PID;
	cgi_process.channel[0] = INVALID_SOCKET;
	cgi_process.channel[1] = INVALID_SOCKET;

	data.fs_path = fs_path;
	data.request = request;

	sus_set_default_headers(&response);

	if (sus_create_process(&cgi_process, sus_execute_cgi, &data) == SUS_ERROR) {
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

static int sus_run_static(int fd, const request_t *request, const char *fs_path)
{
	int static_file_fd;
	response_t response;

	memset(&response, 0, sizeof(response_t));

	response.content_type = sus_get_content_type(fs_path); 

	if (request->accept_encoding) {
		if (strstr(request->accept_encoding, "gzip")) {
			response.supported_encodings |= GZIP;
		}
		if (strstr(request->accept_encoding, "deflate")) {
			response.supported_encodings |= DEFLATE;
		}
		if (strstr(request->accept_encoding, "br")) {
			response.supported_encodings |= BR;
		}
	}

	sus_set_default_headers(&response);

	static_file_fd = open(fs_path, O_RDONLY);
	if (static_file_fd == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"open()\" %s: %s", fs_path, strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (sus_response_from_fd(static_file_fd, &response) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (sus_send_response(fd, &response) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (close(static_file_fd) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	sus_fre_res(&response);
	return SUS_OK;
}

static int sus_kgo_response(int fd, const request_t *request)
{
#define LINUX_MAX_PATHLEN 4096
	int ret;
	char fs_path[LINUX_MAX_PATHLEN];

	memset(fs_path, 0, LINUX_MAX_PATHLEN);

	if (sus_create_fspath(fs_path, request) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (!sus_file_exists(fs_path)) {
		sus_send_response_error(fd, HTTP_NOT_FOUND);
		return SUS_OK;
	}

	if (request->cgi) {
		ret = sus_run_cgi(fd, request, fs_path);
	} else {
		ret = sus_run_static(fd, request, fs_path);
	}

	if (ret == SUS_ERROR && sus_errno != 0) {
		sus_send_response_error(fd, sus_errno);
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int sus_success_callback(int fd, char *buf, int rc)
{
	request_t request;
	int ret;

	memset(&request, 0, sizeof(request_t));

	ret = sus_parse_request(buf, &request);
	if (ret == SUS_ERROR) {
		goto error;
	}

	sus_dump_request(&request); // dumps request to screen with DEBUG defined
	sus_log_access(&request, rc); // dumps request to access.log without defined DEBUG

	if (sus_kgo_response(fd, &request) == SUS_ERROR) {
		goto error;
	}

	sus_fre_req(&request);
	return SUS_OK;
error:
	sus_fre_req(&request);
	return SUS_ERROR;
}

static int sus_disconnect_callback(int *fd)
{
#ifdef DEBUG
	fprintf(stdout, "%d disconnected\n", *fd);
#endif
	sus_query_disconnect(fd);
	return SUS_OK;
}

static int sus_error_callback(int fd)
{
	sus_log_error(LEVEL_PANIC, "Failed to read fd %d: %s", fd, strerror(errno));
	return SUS_OK;
}

typedef int (*PFD_ERROR)(int fd);
typedef int (*PFD_SUCCESS)(int fd, char *buf, int rc);
typedef int (*PFD_DISCONNECTED)(int *fd);

static void sus_cycle_cons(
		PFD_ERROR fd_error,
		PFD_SUCCESS fd_success,
		PFD_DISCONNECTED fd_disconnected)
{
	int n, i, rc, client_disconnected;
	char buf[REQUEST_BUFSIZE];

	n = nfds;
	client_disconnected = 0;
	for (i = LISTEN_FD+1; i < n; i++) {
		if (fds[i].revents & POLLIN) {
			/* NOTE can read fd and handle it */
			rc = sus_read_clientfd(fds[i].fd, buf, WORKER_BUFSIZE);

			switch (rc) {
				case SUS_ERROR:
					fd_error(fds[i].fd);
					break;
				case SUS_DISCONNECTED:
					fd_disconnected(&fds[i].fd);
					client_disconnected = 1;
					break;
				default:
					fd_success(fds[i].fd, buf, rc);
					break;
			}
		} else if (fds[i].revents & POLLERR) {
			sus_log_error(LEVEL_PANIC, "Socket %d received POLLERR", fds[i].fd);
			sus_query_disconnect(&fds[i].fd);
			client_disconnected = 1;
		}
	}

	if (client_disconnected) {
		client_disconnected = 0;
		sus_align_fds();
	}
}

static void sus_start_worker(int bridge_fd, void *data)
{
	int n, ret, timeout;

	signal(SIGINT, sus_set_signal);
	sus_addfd_fds(bridge_fd);

	timeout = sus_get_config_polltimeout();
	
	for ( ;; ) {
		n = poll(fds, nfds, timeout);
		switch (n) {
		case -1:
			if (errno != EINTR) {
				sus_log_error(LEVEL_PANIC, "Failed \"poll()\" in worker: %s", strerror(errno));
				exit(1);
			}
			goto signals; /* got EINTR */
		case 0:
			/* timeout */
			continue;
		}

		if (fds[LISTEN_FD].revents & POLLIN) {
			/* worker can recvmsg from the bridge */
			ret = sus_process_bridge_data(fds[LISTEN_FD].fd);
			if (ret == SUS_ERROR || ret == SUS_DISCONNECTED) {
				goto exit;
			}
		}

		if (nfds > 1) {
			sus_cycle_cons(sus_error_callback, sus_success_callback, sus_disconnect_callback);
		}

signals:
		if (SIGINT_RECV) {
			SIGINT_RECV = 0;
			break;
		}
		if (SIGHUP_RECV) {
			SIGHUP_RECV = 0;
		}
		if (SIGCHLD_RECV) {
			SIGCHLD_RECV = 0;
		}
	}
exit:
	exit(SUS_OK);
}

int sus_init_workers()
{
	int i;
	process_t worker_proc;
	
	workerslen = sus_get_config_workers();

	for (i = 0; i < workerslen; i++) {
		if (sus_create_process(&worker_proc, sus_start_worker, NULL) == SUS_ERROR) {
			return SUS_ERROR;
		}
		workers[i] = worker_proc;
	}
	return SUS_OK;
}
