#include "process.h"

process_t create_process(void (*run)(int fd, void *data), void *data)
{
	process_t proc;
	proc.pid = -1;

	pid_t pid;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, proc.channel) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"socketpair()\": %s", strerror(errno));
		return proc;
	}
#ifdef DEBUG
	printf("after socketpair: %d-%d\n", proc.channel[0], proc.channel[1]);
#endif

	pid = fork();
	switch (pid) {
		case -1:
			sus_log_error(LEVEL_PANIC, "Failed \"fork()\": %s", strerror(errno));
			break;
		case 0:
			if (close(proc.channel[0]) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"close()\" in child: %s", strerror(errno));
				exit(1);
			}
			proc.channel[0] = INVALID_SOCKET;

			run(proc.channel[1], data);
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
#ifdef DEBUG
	printf("Created proc %d\n", proc.pid);
#endif
	return proc;
}

int wait_process(const process_t *process)
{
	/* NOTE wait_process() returns wstatus of awaited process */
	// TODO
	pid_t pid;
	int wstatus;
	
	pid = waitpid(process->pid, &wstatus, WUNTRACED);
	if (pid == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"waitpid()\": %s", strerror(errno));
		return SUS_ERROR;
	}

	// TODO check return code of the process

	return wstatus;
}
