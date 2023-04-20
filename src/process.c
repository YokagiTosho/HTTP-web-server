#include "process.h"

int sus_create_process(process_t *proc, void (*run)(int fd, void *data), void *data)
{
	pid_t pid;

	proc->channel[0] = INVALID_SOCKET;
	proc->channel[1] = INVALID_SOCKET;
	proc->pid = INVALID_PID;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, proc->channel) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"socketpair()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	/* NOTE making UNIX sockets non-blocking because otherwise it won't work with CGI.
	 * And the rest I don't care: in other ways there's 'poll()' used */
	//fcntl(proc->channel[0], F_SETFL, O_NONBLOCK);
	fcntl(proc->channel[1], F_SETFL, O_NONBLOCK);

#if 0
#ifdef DEBUG
	fprintf(stdout, "after socketpair: %d-%d\n", proc.channel[0], proc.channel[1]);
#endif
#endif

	pid = fork();
	switch (pid) {
		case -1:
			sus_log_error(LEVEL_PANIC, "Failed \"fork()\": %s", strerror(errno));
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
		case 0:
			if (close(proc->channel[0]) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"close()\" in child: %s", strerror(errno));
				exit(1);
			}
			proc->channel[0] = INVALID_SOCKET;

			run(proc->channel[1], data);
			// NOTE usually should not be reached, 'run' callback should contain its own mechanism to exit
			exit(1);
			break;
		default:
			if (close(proc->channel[1]) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"close()\" in the caller: %s", strerror(errno));
				sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
				return SUS_ERROR;
			}
			break;
	}
	proc->pid = pid;
	proc->channel[1] = INVALID_SOCKET;

#ifdef DEBUG
	fprintf(stdout, "Created proc %d\n", proc->pid);
#endif

	return SUS_OK;
}

int sus_wait_process(const process_t *process)
{
	pid_t pid;
	int wstatus;
	
	pid = waitpid(process->pid, &wstatus, WUNTRACED);
	if (pid == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"waitpid()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}
	return wstatus;
}

int sus_close_process(process_t *process)
{
	if (process->channel[0] != INVALID_SOCKET) {
		if (close(process->channel[0]) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
			return SUS_ERROR;
		}
	}
	if (process->channel[1] != INVALID_SOCKET) {
		if (close(process->channel[1]) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
			return SUS_ERROR;
		}
	}
	return SUS_OK;
}
