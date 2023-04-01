#include "bridge.h"

#include "BW_protocol.h"

static int SIGINT_RECV, SIGHUP_RECV, SIGCHLD_RECV;
static void handle_signal(int sig)
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

static void bridge_run(int main_fd, void *data);

int init_bridge(process_t *proc)
{
	*proc = create_process(bridge_run, NULL);
	if (proc->pid == INVALID_PID) {
		return SUS_ERROR;
	}
	return SUS_OK;
}

#if 0
static const worker_t *find_free_worker(const a_workers_t *workers)
{
	/* Sends protocol code to worker to see if someone is not busy(free slots for new socket).
	 * If none is available return NULL */
	int n;
	int i;
	char code;
	struct { int busy; int max; } response;

	n = workers->workerslen;
	code = WORKER_GET_CONNECTIONS;
	for (i = 0; i < n; i++) {
		const worker_t *worker = &workers->workers[i];
		if (write(worker->socket, &code, sizeof(char)) != sizeof(char)) {
			sus_log_error(LEVEL_PANIC, "Failed to write() to worker on the bridge: %s\n", strerror(errno));
			continue;
		}
		if (read(worker->socket, &response, sizeof(response)) != sizeof(response)) {
			sus_log_error(LEVEL_PANIC, "Failed to read() from worker on the bridge: %s\n", strerror(errno));
			continue;
		}
		if (response.busy < response.max) {
			/* TODO retinker this stuff to be more optimizing */
#if 0
			fprintf(stderr, "DEBUG: Got from worker: %d/%d\n", response.busy, response.max);
			fprintf(stderr, "DEBUG: Sending socket to %d\n", worker->pid);
#endif
			return worker;
		}
	}
	return NULL;
}
#endif

static int fdacchndl(int fd)
{
	/* fdacchndl accepts new client socket and sends it to available worker.
	 * In this case sending only to workers[0].
	 * TODO algorithm to determine who to send file descriptor! */
	int new_socket;
	const process_t *process = &workers[0];
	if ((new_socket = accept(fd, NULL, NULL)) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed accept(): %s", strerror(errno));
		return SUS_ERROR;
	}

	channel_t ch;
	ch.cmd = WORKER_RECV_FD;
	ch.socket = new_socket;

	if (send_fd(process->channel[0], &ch, sizeof(channel_t)) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed send_fd(): %s", strerror(errno));
		return SUS_ERROR;
	}

	if (close(new_socket)) {
		sus_log_error(LEVEL_PANIC, "Failed close() in 'fdacchndl': %s", strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

/* The only purpose of the bridge is to accept new sockets
 * and send them to workers, with a little bit of communication for sync. 
 * That's it, no more - no less. 
 */
static void bridge_run(int main_fd, void *data)
{
	signal(SIGINT, handle_signal);

	int i, n, server_fd, nfds;
	struct pollfd fds[MAX_WORKERS]; 
	
	if ((server_fd = server_fd_init()) == SUS_ERROR) {
		exit(1);
	}
	
	nfds = workerslen+1+1; /* 1 and 1 for server->socket and main_fd respectively */

	fds[0].fd = server_fd;
	fds[0].events = POLLIN;

	fds[1].fd = main_fd;
	fds[1].events = POLLIN;

	for (i = 0; i < workerslen; i++) {
		fds[i+2].fd = workers[i].channel[0];
		fds[i+2].events = POLLIN;
	}

	for ( ;; ) {
		n = poll(fds, nfds, 10000);
		switch (n) {
			case -1:
				if (errno != EINTR) {
					sus_log_error(LEVEL_PANIC, "Failed poll() on the bridge: %s", strerror(errno));
					exit(1);
				} else {
					goto signals;
				}
			case 0:
				/* timeout */
				continue;
		}

		if (fds[0].revents & POLLIN) {
			/* NOTE can read server socket */
			fdacchndl(fds[0].fd);
		}
		else if (fds[1].revents & POLLIN) {
			/* NOTE parent proc socket 
			 * Decide to keep this fd or not. 
			 * Usually I do not need communication with master proc */
		}

		n = nfds;
		for (i = 2; i < n; i++) {
			if (fds[i].revents & POLLIN) {
				/* NOTE can read worker */
			} else if (fds[i].revents & POLLERR) {
				sus_log_error(LEVEL_PANIC, "%d POLLERR", fds[i].fd);
				continue;
			}
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

	close(server_fd);
	close(main_fd);

	exit(0);
}
