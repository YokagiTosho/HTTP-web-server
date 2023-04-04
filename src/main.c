#include "base.h"

#include "config.h"
#include "bridge.h"
#include "stderr.h"
#include "worker.h"

#include "log.h"

static int SIGINT_RECV, SIGHUP_RECV, SIGCHLD_RECV;
static void sus_handle_signal(int sig)
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

static int sus_kill_processes_sig(int sig)
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

static int sus_process_signal()
{
	int wstatus;
	pid_t pid;

	if (SIGINT_RECV) {
		SIGINT_RECV = 0;
		sus_kill_processes_sig(SIGINT);
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
				/* TODO remove awaited worker from global var 'workers' after */
				/* worker proc */
#ifdef DEBUG
				fprintf(stdout, "waited %d worker\n", pid);
#endif
			} else {
				/* bridge proc */
#ifdef DEBUG
				fprintf(stdout, "waited %d bridge\n", pid);
#endif
				sus_log_error(LEVEL_PANIC, "Bridge died, terminating whole server");
				exit(1);
			}
		}
	}
	return SUS_OK;
}

#include "response.h"

int main(int argc, char **argv)
{
	int ret;

	close(STDIN_FILENO);
	sus_redir_stream("error.log", STDERR_FILENO);
#ifndef DEBUG
	sus_redir_stream("access.log", STDOUT_FILENO);
#endif

	if (sus_parse_config() == SUS_ERROR) {
		exit(1);
	}
	

	if (sus_init_workers() == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed init_workers()");
		exit(1);
	}

	if (sus_init_bridge(&bridge) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed init_workers()");
		exit(1);
	}

	sigset_t set, old;
	sigemptyset(&set);
	sigemptyset(&old);

	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGCHLD);

	signal(SIGINT, sus_handle_signal);
	signal(SIGHUP, sus_handle_signal);
	signal(SIGCHLD, sus_handle_signal);

	if (sigprocmask(SIG_SETMASK, &set, &old) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed at sigprocmask in main proc: %s", strerror(errno));
		exit(1);
	}

	for ( ;; ) {
		sigsuspend(&old);
		ret = sus_process_signal();
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
