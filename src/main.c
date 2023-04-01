#include "base.h"

#include "config.h"
#include "bridge.h"
#include "stderr.h"
#include "worker.h"

#include "log.h"

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

#define TERMINATE 1

static process_t bridge;

static int kill_processes_sig(int sig)
{
	if (kill(bridge.pid, sig) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed kill(): %s", strerror(errno));
		return SUS_ERROR;
	}
	for (int i = 0; i < workerslen; i++) {
		if (kill(workers[i].pid, sig) == -1) {
			sus_log_error(LEVEL_PANIC, "Failed kill(): %s", strerror(errno));
			return SUS_ERROR;
		}
	}
	return SUS_OK;
}

static int process_signal()
{
	int wstatus;
	pid_t pid;

	if (SIGINT_RECV) {
		SIGINT_RECV = 0;
		kill_processes_sig(SIGINT);
		return TERMINATE;
	}
	else if (SIGHUP_RECV) {
		SIGHUP_RECV = 0;
	}
	else if (SIGCHLD_RECV) {
		/* TODO check return status code of child */
		SIGCHLD_RECV = 0;
		pid = wait(&wstatus);
		if (pid == -1) {
			sus_log_error(LEVEL_PANIC, "Failed \"wait()\": %s", strerror(errno));
			return SUS_ERROR;
		} else {
			if (pid != bridge.pid) {
				/* worker proc */
#ifdef DEBUG
				printf("waited %d worker\n", pid);
#endif
			} else {
				/* bridge proc */
#ifdef DEBUG
				printf("waited %d bridge\n", pid);
#endif
			}
		}
	}

	return SUS_OK;
}

int main(int argc, char **argv)
{
	close(STDIN_FILENO);

	redir_stream("error.log", STDERR_FILENO);
#ifndef DEBUG
	redir_stream("access.log", STDOUT_FILENO);
#endif

	if (parse_config() == SUS_ERROR) {
		return 1;
	}

	int ret;

	if (init_workers() == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed init_workers()");
		exit(1);
	}

	if (init_bridge(&bridge) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed init_workers()");
		exit(1);
	}

	sigset_t set, old;
	sigemptyset(&set);
	sigemptyset(&old);

	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGCHLD);

	signal(SIGINT, handle_signal);
	signal(SIGHUP, handle_signal);
	signal(SIGCHLD, handle_signal);

	if (sigprocmask(SIG_SETMASK, &set, &old) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed at sigprocmask in main proc: %s", strerror(errno));
		exit(1);
	}

	for ( ;; ) {
		sigsuspend(&old);
		ret = process_signal();
		switch (ret) {
			case SUS_ERROR:
				sus_log_error(LEVEL_PANIC, "Failed process_signal()");
				goto exit;
				break;
			case TERMINATE:
				goto exit;
				break;
		}
	}
exit:
	return 0;
}
