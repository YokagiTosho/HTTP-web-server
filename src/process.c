#include "process.h"

process_t create_process(void (*run)(int fd, void *data), void *data)
{
	process_t proc;
	pid_t pid;

	proc.channel[0] = INVALID_SOCKET;
	proc.channel[1] = INVALID_SOCKET;
	proc.pid = -1;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, proc.channel) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"socketpair()\": %s", strerror(errno));
		return proc;
	}
#if 0
#ifdef DEBUG
	fprintf(stdout, "after socketpair: %d-%d\n", proc.channel[0], proc.channel[1]);
#endif
#endif

	pid = fork();
	switch (pid) {
		case -1:
			sus_log_error(LEVEL_PANIC, "Failed \"fork()\": %s", strerror(errno));
		case 0:
			if (close(proc.channel[0]) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"close()\" in child: %s", strerror(errno));
				exit(1);
			}
			proc.channel[0] = INVALID_SOCKET;

			run(proc.channel[1], data);
			// NOTE usually should not be reached, 'run' callback should contain its own mechanism to exit
			exit(0);
			break;
		default:
			if (close(proc.channel[1]) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"close()\" in the caller: %s", strerror(errno));
				exit(1);
			}
			break;
	}
	proc.pid = pid;
	proc.channel[1] = INVALID_SOCKET;
#if 1
#ifdef DEBUG
	fprintf(stdout, "Created proc %d\n", proc.pid);
#endif
#endif
	return proc;
}

int wait_process(const process_t *process)
{
	pid_t pid;
	int wstatus;
	
	pid = waitpid(process->pid, &wstatus, WUNTRACED);
	if (pid == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"waitpid()\": %s", strerror(errno));
		return SUS_ERROR;
	}
	return wstatus;
}
