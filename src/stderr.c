#include "stderr.h"

static int redir_fd(int fd1, int fd2)
{
	if (dup2(fd1, fd2) == -1) {
		fprintf(stderr, "Failed \"dup2()\" %d to %d: %s\n", fd1, fd2, strerror(errno));
		exit(1);
	}
	close(fd1);

	return 0;
}

static int open_wronly_file(const char *path)
{
	int fd = open(path, O_CREAT | O_APPEND | O_WRONLY, 0666);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
		exit(1);
	}

	return fd;
}

int redir_stream(const char *log_path, int stream_fd)
{
	int fd = open_wronly_file(log_path);
	redir_fd(fd, stream_fd);

	return 0;
}
