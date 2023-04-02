#include "server.h"

static int sus_server_socket()
{
	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		sus_log_error(LEVEL_PANIC, "Failed to get socket: %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		exit(1);
	}
	return fd;
}

static int sus_server_socket_bind(int fd)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(sus_get_config_port());
	addr.sin_addr.s_addr = sus_get_config_addr(); // already in network byte order

	if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed to bind socket: %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	return SUS_OK;
}

static int sus_server_socket_listen(int fd)
{
	if (listen(fd, 5) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed at \"listen()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}
	return SUS_OK;
}

int sus_server_fd_init()
{
	int fd;

	fd = sus_server_socket();

	if (sus_server_socket_bind(fd) == SUS_ERROR) {
		//sus_log_error(LEVEL_PANIC, "Failed \"server_socket_bind()\"");
		return SUS_ERROR;
	}
	if (sus_server_socket_listen(fd) == SUS_ERROR) {
		//sus_log_error(LEVEL_PANIC, "Failed \"server_socket_listen()\"");
		return SUS_ERROR;
	}

	return fd;
}
