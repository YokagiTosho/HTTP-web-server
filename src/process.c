#include "process.h"

process_t create_process(void (*run)(int fd))
{
	process_t proc;
	proc.pid = -1;

	pid_t pid;
	//int i, n;

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
			run(proc.channel[1]);
			break;
		default:
			if (close(proc.channel[1]) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"close()\" in the caller: %s", strerror(errno));
				exit(1);
			}
			
			break;
	}
	proc.pid = pid;
#ifdef DEBUG
	printf("Created proc %d\n", proc.pid);
#endif
	return proc;
}

int wait_process()
{
	// TODO
}
