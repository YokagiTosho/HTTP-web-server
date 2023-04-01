#include "worker.h"

process_t workers[MAX_WORKERS]; // extern
int workerslen;                 // extern

static struct pollfd fds[MAX_CONNECTIONS];
static int nfds;

static int SIGINT_RECV, SIGHUP_RECV, SIGCHLD_RECV;
static void set_signal(int sig)
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

static int query_disconnect(int *socket)
{
	int s = *socket;
	if (s == INVALID_SOCKET) {
		sus_log_error(LEVEL_PANIC, "In \"query_disconnect\" received \"INVALID_SOCKET\": %d", s);
		return SUS_ERROR;
	}
	if (close(s) == SOCK_ERR) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
		return SUS_ERROR;
	}
	*socket = INVALID_SOCKET;
	return SUS_OK;
}

static int addfd_fds(int fd)
{
	if (nfds == MAX_CONNECTIONS) {
		return SUS_ERROR;
	}

	fds[nfds].fd = fd;
	fds[nfds].events = POLLIN;
	nfds++;

	return SUS_OK;
}

static void align_fds()
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

static int process_bridge_data(int fd)
{
	int rc;
	channel_t ch;

	rc = recv_fd(fd, &ch, sizeof(channel_t));
	if (rc == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (ch.cmd == WORKER_RECV_FD) {
		if (addfd_fds(ch.socket) == SUS_ERROR) {
			/* TODO send error to client */
			sus_log_error(LEVEL_PANIC, "Reached maximum(%d) available sockets", MAX_CONNECTIONS);

			/* TODO prepare here response with error message and send it to the client */

			close(ch.socket);
			return SUS_ERROR;
		}
	}

	return SUS_OK;
}

static int read_clientfd(int fd, char *buf, int size)
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

/* NOTE with callbacks */
static int error_callback(int fd)
{
	sus_log_error(LEVEL_PANIC, "Failed to read fd %d: %s", fd, strerror(errno));
	return SUS_OK;
}

static int file_exists(const char *filepath)
{
	return access(filepath, F_OK | R_OK) != -1;
}

static int create_fspath(char *fs_path, const request_t *request)
{
	int i, args_st;
	char *p;

	strcpy(fs_path, "examples/"); // TODO change this to config variable, HARDCODED for test
	if (request->args) {
		p = strchr(request->uri, '?');
		if (!p) { 
			/* should not ever happen */
			return SUS_ERROR;
		}
		int fs_pathend = strlen(fs_path)+1;
		args_st = (int)(p - request->uri);
		for (i = 0; i < args_st; i++) {
			fs_path[i+fs_pathend] = request->uri[i];
		}
		fs_path[i+fs_pathend] = '\0';
	} else {
		strcat(fs_path, request->uri);
	}

	if (request->dir && !request->cgi) {
		strcat(fs_path, "index.html"); // TODO take from configuration as well
	} else if (request->dir && request->cgi){
		strcat(fs_path, "index.cgi"); // TODO take from configuration as well
	}

	return SUS_OK;
}

static int run_cgi(int fd, const request_t *request, const char *fs_path)
{
	/* Runs CGI script, parses its output, makes response and sends it to fd */
	process_t cgi_process;
	response_t response;
	struct cgi_data data;

	/* TODO prepare ENV from *request */

	memset(&response, 0, sizeof(response_t));

	cgi_process.pid = INVALID_PID;
	cgi_process.channel[0] = INVALID_SOCKET;
	cgi_process.channel[1] = INVALID_SOCKET;

	/* TODO data preparation, fill env with request variables and set its length */
	data.fs_path = fs_path;
	data.env = NULL;
	data.envlen = 0;

	// TODO prepare ENV for cgi, call cgi, grab its output and send response
	cgi_process = create_process(execute_cgi, &data);
	if (cgi_process.pid == INVALID_PID) {
		sus_log_error(LEVEL_PANIC, "Failed \"create_process()\"");
		return SUS_ERROR;
	}

	if (wait_process(&cgi_process) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"wait_process()\"");
		return SUS_ERROR;
	}

	if (response_from_fd(cgi_process.channel[0], &response) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"response_from_fd()\"");
		return SUS_ERROR;
	}

	/* send here response */
	if (send_response(fd, &response) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"send_response()\"");
		return SUS_ERROR;
	}

	if (close(cgi_process.channel[0]) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
	}

	fre_res(&response);
	return SUS_OK;
}

static int run_static(int fd, const request_t *request, const char *fs_path)
{
	/* Loads static file, makes response and sends it to fd 
	 * Also static files should be cached if file is hot. 
	 * Look for Transfer-Encoding: chunked for big files */
	int static_file_fd;
	response_t response;

	memset(&response, 0, sizeof(response_t));

	response.content_type = get_content_type(fs_path); 

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

	set_default_headers(&response);

	static_file_fd = open(fs_path, O_RDONLY);
	if (static_file_fd == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"open()\" %s: %s", fs_path, strerror(errno));
		return SUS_ERROR;
	}

	if (response_from_fd(static_file_fd, &response) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"response_from_fd()\"");
		return SUS_ERROR;
	}

	if (send_response(fd, &response) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"send_response()\"");
		return SUS_ERROR;
	}

	if (close(static_file_fd) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
	}

	fre_res(&response);

	return SUS_OK;
}

static int kgo_response(int fd, const request_t *request)
{
#define LINUX_MAX_PATHLEN 4096
	char fs_path[LINUX_MAX_PATHLEN];
	memset(fs_path, 0, LINUX_MAX_PATHLEN);

	if (create_fspath(fs_path, request) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"create_fspath()\"");
		return SUS_ERROR;
	}

#ifdef DEBUG
	printf("fs_path: %s\n", fs_path);
#endif

	if (!file_exists(fs_path)) {
		char not_found_msg[] = 
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/html\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: 182\r\n"
			"\r\n"
			"<html>"
			"<head><title>404 Not Found</title></head>"
			"<body>"
			"<h1>Not Found</h1></body>"
			"<p>The requested URL was not found on this server</p>"
			"<hr>"
			"<p>Simple Unattractive Server</p>"
			"</body>"
			"</html>";
		send_hardcoded_msg(fd, not_found_msg, sizeof(not_found_msg)-1);
		return SUS_OK;
	}

	/* TODO here some checks, if only headers must be send, etc. */
	/* TODO fill here general response_t fields, like server, date etc. */

	if (request->cgi) {
		run_cgi(fd, request, fs_path);
	}
	else {
		if (run_static(fd, request, fs_path) == SUS_ERROR) {
			/* TODO look for global var sus_errno and send client an error */
		}
	}

	return SUS_OK;
}

static int success_callback(int fd, char *buf, int rc)
{
#define URI_MAXLEN 257
	/* NOTE this function will be called on successfull read client's request */
	request_t request;
	int ret;

	memset(&request, 0, sizeof(request_t));

	ret = parse_request(buf, &request);
	if (ret == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"parse_request()\"");
		goto error;
	}

#if 1
	dump_request(&request); // dumps request to screen with DEBUG
#endif
	log_access(&request, rc); // dumps request to access.log without DEBUG

	/* TODO Prepare response */
	if (kgo_response(fd, &request) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"handle_request()\"");
		goto error;
	}

	fre_req(&request);
	return SUS_OK;
error:
	fre_req(&request);
	return SUS_ERROR;
}

static int disconnect_callback(int *fd)
{
#ifdef DEBUG
	printf("%d disconnected\n", *fd);
#endif
	query_disconnect(fd);
	return SUS_OK;
}

static void cycle_cons(
		int (*fd_error)(int fd),
		int (*fd_success)(int fd, char *buf, int rc),
		int (*fd_disconnected)(int *fd))
{
	int n, i, rc, client_disconnected;
	char buf[REQUEST_BUFSIZE];

	n = nfds;
	client_disconnected = 0;
	for (i = LISTEN_FD+1; i < n; i++) {
		if (fds[i].revents & POLLIN) {
			/* NOTE can read fd and handle it */
			rc = read_clientfd(fds[i].fd, buf, WORKER_BUFSIZE);

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
		}
		else if (fds[i].revents & POLLERR) {
			sus_log_error(LEVEL_PANIC, "Socket %d received POLLERR", fds[i].fd);
			query_disconnect(&fds[i].fd);
			client_disconnected = 1;
		}
	}

	if (client_disconnected) {
		client_disconnected = 0;
		align_fds();
	}
}

static void start_worker(int bridge_fd, void *data) 
{
#define TIMEOUT 10000
	signal(SIGINT, set_signal);
	addfd_fds(bridge_fd);
	int n;
	
	for ( ;; ) {
		n = poll(fds, nfds, TIMEOUT);
		switch (n) {
		case -1:
			if (errno != EINTR) {
				sus_log_error(LEVEL_PANIC, "Failed \"poll()\" in worker: %s", strerror(errno));
				exit(1);
			} goto signals; /* got EINTR */
		case 0:
			/* timeout
			 * TODO close fds that didn't have data, expect bridge_fd */
			continue;
		}

		if (fds[LISTEN_FD].revents & POLLIN) {
			/* worker can recvmsg from the bridge */
			process_bridge_data(fds[LISTEN_FD].fd);
		}

		if (nfds > 1) {
			cycle_cons(error_callback, success_callback, disconnect_callback);
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
	exit(SUS_OK);
}

int init_workers()
{
	int i;

	workerslen = get_config_workers();
	for (i = 0; i < workerslen; i++) {
		process_t worker_proc = create_process(start_worker, NULL);
		if (worker_proc.pid == INVALID_PID) {
			return SUS_ERROR;
		}
		workers[i] = worker_proc;
	}
	return SUS_OK;
}
