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
	response_t response;

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

#define CALLBACK_CYCLE_CONS

#ifndef CALLBACK_CYCLE_CONS
static void cycle_cons()
{
	int n, i, rc, ret, client_disconnected;
	char buf[REQUEST_BUFSIZE];
	request_t request;

	n = nfds;
	client_disconnected = 0;
	for (i = LISTEN_FD+1; i < n; i++) {
		if (fds[i].revents & POLLIN) {
			/* NOTE can read fd and handle it */
			rc = read_clientfd(fds[i].fd, buf, WORKER_BUFSIZE);
			switch (rc) {
				case SUS_ERROR:
					sus_log_error(LEVEL_PANIC, "Failed to read fd %d: %s", fds[i].fd, strerror(errno));
					break;
				case SUS_DISCONNECTED:
#ifdef DEBUG
					printf("%d disconnected\n", fds[i].fd);
#endif
					if (query_disconnect(&fds[i].fd) == SUS_ERROR) {
						sus_log_error(LEVEL_PANIC, "Failed \"query_disconnect()\"");
						continue;
					}
					client_disconnected = 1;
					break;
				default:
					memset(&request, 0, sizeof(request_t));
					ret = parse_request(buf, &request);
					if (ret == SUS_ERROR) {
						sus_log_error(LEVEL_PANIC, "Failed \"parse_request()\"");
						continue;
					}
					dump_request(&request); // dumps request to screen with DEBUG
					log_access(&request, rc); // dumps request to access.log without DEBUG
#if 0
					char test_answer[] = 
						"HTTP/1.1 200 OK\r\n"
						"Content-Type: text/html\r\n"
						"Connection: keep-alive\r\n"
						"Content-Length: 29\r\n"
						"\r\n"
						"<h1>Test msg from server</h1>";
					if (write(fds[i].fd, test_answer, sizeof(test_answer)-1) != sizeof(test_answer)-1) {
						sus_log_error(LEVEL_WARNING, "write != sizeof(test_answer): %s", strerror(errno));
					}
#endif
					/* TODO Prepare response
					 * and send it back to the user */
					fre_req(&request);
					break;
			}
		}
		else if (fds[i].revents & POLLERR) {
			sus_log_error(LEVEL_PANIC, "Socket %d received POLLERR", fds[i].fd);
			if (query_disconnect(&fds[i].fd) == SUS_ERROR) {
				sus_log_error(LEVEL_PANIC, "Failed \"query_disconnect()\"");
			}
		}
	}

	if (client_disconnected) {
		client_disconnected = 0;
		align_fds();
	}
}

#else

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
	int i, j, args_st;
	char *p;

	strcpy(fs_path, "./examples/"); // TODO change this to config variable, HARDCODED for test
	if (request->args) {
		p = strchr(request->uri, '?');
		if (!p) { 
			/* should not ever happen */
			return SUS_ERROR;
		}
		args_st = (int)(p - request->uri);
		for (i = 0, j = strlen(fs_path)+1; i < args_st; i++, j++) {
			fs_path[j] = request->uri[i];
		}
		fs_path[j] = '\0';
	} else {
		strcat(fs_path, request->uri);
	}

	if (request->dir && !request->cgi) {
		strcat(fs_path, "index.html");
	} else if (request->dir && request->cgi){
		strcat(fs_path, "index.cgi");
	}

	return SUS_OK;
}

static int success_callback(int fd, char *buf, int rc)
{
#define LINUX_MAX_PATHLEN 4096
#define URI_MAXLEN 257
	/* NOTE this function will be called on successfull read client's request */
	request_t request;
	response_t response;
	int ret;

	memset(&request, 0, sizeof(request_t));
	memset(&response, 0, sizeof(response_t));

	ret = parse_request(buf, &request);
	if (ret == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"parse_request()\"");
		return SUS_ERROR;
	}
	dump_request(&request); // dumps request to screen with DEBUG
	log_access(&request, rc); // dumps request to access.log without DEBUG

#if 1
	char fs_path[LINUX_MAX_PATHLEN];
	memset(fs_path, 0, LINUX_MAX_PATHLEN);
	
	if (create_fspath(fs_path, &request) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"create_fspath()\"");
		return SUS_ERROR;
	}

	if (!file_exists(fs_path)) {
		/* TODO send 404 */
		return SUS_ERROR;
	}

	if (request.cgi) {
		// TODO call cgi, grab its output and send response
	} else {
		// TODO load file and send response
	}

#ifdef DEBUG
	printf("%s\n", fs_path);
#endif

#endif

	/* TODO Prepare response
	 * and send it back to the user */
	send_response(fd); // test function
	
	fre_req(&request);
	fre_res(&response);

	return SUS_OK;
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
	int n, i, rc, ret, client_disconnected;
	char buf[REQUEST_BUFSIZE];

	n = nfds;
	client_disconnected = 0;
	for (i = LISTEN_FD+1; i < n; i++) {
		if (fds[i].revents & POLLIN) {
			/* NOTE can read fd and handle it */
			rc = read_clientfd(fds[i].fd, buf, WORKER_BUFSIZE);
			switch (rc) {
				case SUS_ERROR:
					ret = fd_error(fds[i].fd);
					break;
				case SUS_DISCONNECTED:
					ret = fd_disconnected(&fds[i].fd);
					client_disconnected = 1;
					break;
				default:
					ret = fd_success(fds[i].fd, buf, rc);
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
#endif

static void start_worker(int bridge_fd) 
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
#ifndef CALLBACK_CYCLE_CONS
			cycle_cons();
#else
			cycle_cons(error_callback, success_callback, disconnect_callback);
#endif
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
		process_t worker_proc = create_process(start_worker);
		if (worker_proc.pid == INVALID_PID) {
			return SUS_ERROR;
		}
		workers[i] = worker_proc;
	}
	return SUS_OK;
}
