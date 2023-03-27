#include "server.h"

static int server_socket()
{
	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		sus_log_error(LEVEL_PANIC, "Failed to get socket: %s", strerror(errno));
		exit(1);
	}
	return fd;
}

static int server_socket_bind(int fd)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(get_config_port());
	addr.sin_addr.s_addr = htonl(get_config_addr());

	if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to bind socket: %s", strerror(errno));
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int server_socket_listen(int fd)
{
	if (listen(fd, 5) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed at \"listen()\": %s", strerror(errno));
		return SUS_ERROR;
	}
	return SUS_OK;
}

int server_fd_init()
{
	int fd;

	fd = server_socket();

	if (server_socket_bind(fd) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"server_socket_bind()\"");
		return SUS_ERROR;
	}
	if (server_socket_listen(fd) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"server_socket_listen()\"");
		return SUS_ERROR;
	}

	return fd;
}
