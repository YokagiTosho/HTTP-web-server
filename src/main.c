#include "base.h"

#include "config.h"
#include "bridge.h"
#include "stderr.h"
#include "worker.h"

#include "log.h"

// SUS_PID_PATH is the path where master PID is stored
#define SUS_PID_PATH "/tmp/sus.pid"
#define PIDLEN 64

#define SUS_TERMINATE 1
#define SUS_RESTART   2

static int received_signal;

static sigset_t set, old;

static int sus_write_master_pid()
{
	char pid[PIDLEN];
	int ret;

	ret = snprintf(pid, PIDLEN, "%d\n", getpid());
	if (ret < 0) {
		return -1;
	} else if (ret >= PIDLEN) {
		return -1;
	}

	int filefd = open(SUS_PID_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (filefd == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to open \"%s\" for writing: %s", SUS_PID_PATH, strerror(errno));
		return -1;
	}

	ret = write(filefd, pid, ret);
	if (ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to write \"%s\": %s", SUS_PID_PATH, strerror(errno));
		close(filefd);
		return -1;
	}

	close(filefd);
	return 0;
}

static pid_t sus_read_master_pid()
{
	char pid[PIDLEN];
	int ret;

	int filefd = open(SUS_PID_PATH, O_RDONLY);
	if (filefd == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to open \"%s\" for reading: %s", SUS_PID_PATH, strerror(errno));
		return -1;
	}

	ret = read(filefd, pid, PIDLEN);
	if (ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to read \"%s\": %s", SUS_PID_PATH, strerror(errno));
		close(filefd);
		return -1;
	}

	close(filefd);
	return atoi(pid);
}

static int sus_delete_master_pid()
{
	int ret;

	ret = unlink(SUS_PID_PATH);
	if (ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"unlink()\": %s", strerror(errno));
	}
	
	return ret;
}

static void sus_signal(int sig)
{
	received_signal = sig;
}

static int sus_kill_wait_proc(pid_t pid, int sig)
{
	pid_t awaited_proc;
	int wstatus;

	if (kill(pid, sig) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"kill()\": %s", strerror(errno));
		return SUS_ERROR;
	}
	
	awaited_proc = waitpid(pid, &wstatus, 0);
	if (awaited_proc == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"waitpid()\": %s", strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int sus_kill_processes(int sig)
{
	int i;

	if (sus_kill_wait_proc(bridge.pid, sig) == SUS_ERROR) {
		return SUS_ERROR;
	}
	sus_close_process(&bridge);

	for (i = 0; i < workerslen; i++) {
		if (sus_kill_wait_proc(workers[i].pid, sig) == SUS_ERROR) {
			sus_log_error(LEVEL_PANIC, "Failed kill(): %s", strerror(errno));
			return SUS_ERROR;
		}
		sus_close_process(&workers[i]);
	}
	return SUS_OK;
}

static int sus_handle_signal()
{
	int wstatus, ret;
	pid_t pid;

	switch (received_signal) {
		case SIGINT:
			return SUS_TERMINATE;
		case SIGHUP:
			return SUS_RESTART;
		case SIGCHLD:
			pid = wait(&wstatus);
			if (pid == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"wait()\": %s", strerror(errno));
				return SUS_ERROR;
			}
			if (pid != bridge.pid) {
				/* TODO remove awaited worker from global var 'workers' after */
				/* worker proc */
				if (WIFEXITED(wstatus)) {
					ret = WEXITSTATUS(wstatus);
					if (ret != 0) {
						sus_log_error(LEVEL_PANIC, "Worker %d returned with %d", pid, ret);
					}
				} else if (WIFSIGNALED(wstatus)) {
					sus_log_error(LEVEL_PANIC, "Worker %d was killed by signal: %d", pid, WTERMSIG(wstatus));
				} else if (WIFSTOPPED(wstatus)) {
					sus_log_error(LEVEL_PANIC, "Worker %d was stopped by signal: %d", pid, WSTOPSIG(wstatus));
				}
				/* TODO remove from workers */
			} else {
				/* bridge proc */
				if (WIFEXITED(wstatus)) {
					ret = WEXITSTATUS(wstatus);
					if (ret != 0) {
						sus_log_error(LEVEL_PANIC, "Bridge returned with %d", ret);
					}
				} else if (WIFSIGNALED(wstatus)) {
					sus_log_error(LEVEL_PANIC, "Bridge was killed by signal: %d", WTERMSIG(wstatus));
				} else if (WIFSTOPPED(wstatus)) {
					sus_log_error(LEVEL_PANIC, "Bridge was stopped by signal: %d", WSTOPSIG(wstatus));
				}
				sus_log_error(LEVEL_PANIC, "Bridge died, terminating whole server");
				exit(1);
			}
			break;
	}
	return SUS_OK;
}

static int sus_restart_server()
{
	sus_kill_processes(SIGINT);

	if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed at \"sigprocmask()\" in main process: %s", strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

static void sus_parse_args(int argc, char **argv)
{
	if (argc > 1) {
		pid_t master_pid = sus_read_master_pid();
		if (master_pid == -1) {
#ifdef DEBUG
			fprintf(stderr, "SUS server is not running\n");
#endif
			exit(1);
		} else if (master_pid <= 0) {
#ifdef DEBUG
			fprintf(stderr, "Read invalid master PID: %d\n", master_pid);
#endif
			exit(1);
		}
		if (!strcmp("stop", argv[1])) {
			kill(master_pid, SIGINT);
		} else if (!strcmp("restart", argv[1])) {
			kill(master_pid, SIGHUP);
		} else {
#ifdef DEBUG
			fprintf(stderr, "Unrecognized option: %s\n", argv[1]);
#endif
			exit(1);
		}
		exit(0);
	}
}

static void sus_init_heap()
{
#if 0
#define HEAP_SIZE 1024*1024*8
	void *ptr = malloc(HEAP_SIZE);
	if (!ptr) {
		sus_log_error(LEVEL_PANIC, "Could not allocate memory");
		exit(1);
	}
	free(ptr);
	ptr = NULL;
#endif
}

#include "request.h"

int main(int argc, char **argv)
{
	int ret;

	sus_init_heap();

	sus_parse_args(argc, argv);

	sus_write_master_pid();

	sus_redir_stream("/dev/null", STDIN_FILENO);
	sus_redir_stream("error.log", STDERR_FILENO);
#ifndef DEBUG
	sus_redir_stream("access.log", STDOUT_FILENO);
#endif

	/* TODO remove restart label and make 'sus_parse_config' very first call.
	 * In signal handling just call 'sus_parse_config' again and goto 'sus_init_workers'
	 * This is because redirection of streams happens before config parsing, it should not be this way
	 * */
restart:
	if (sus_parse_config() == SUS_ERROR) {
		exit(1);
	}

	if (sus_init_workers() == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"init_workers()\"");
		exit(1);
	}

	if (sus_init_bridge(&bridge) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"init_bridge()\"");
		exit(1);
	}

	sigemptyset(&set);
	sigemptyset(&old);

	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGCHLD);

	signal(SIGINT, sus_signal);
	signal(SIGHUP, sus_signal);
	signal(SIGCHLD, sus_signal);

	if (sigprocmask(SIG_SETMASK, &set, &old) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed at \"sigprocmask()\" in main proc: %s", strerror(errno));
		exit(1);
	}

	for ( ;; ) {
		sigsuspend(&old);

		ret = sus_handle_signal();
		switch (ret) {
			case SUS_ERROR:
				sus_log_error(LEVEL_PANIC, "Failed \"process_signal()\"");
				goto exit;
			case SUS_TERMINATE:
				sus_kill_processes(SIGINT);
				goto exit;
			case SUS_RESTART:
				ret = sus_restart_server();
				if (ret == SUS_ERROR) {
					goto exit;
				}
				goto restart;
		}
	}

exit:
	sus_delete_master_pid();
	return 0;
}
