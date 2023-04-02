#include "compress.h"

static void sus_compress_gzip_callback(int fd, void *data)
{
	int tmp_fd = *(int *)data;

	// make gzip program read from unix socket fd
	if (dup2(fd, STDIN_FILENO) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"dup2()\": %s", strerror(errno));
		exit(1);
	}
	// redirect gzip output to the new tmp file
	if (dup2(tmp_fd, STDOUT_FILENO) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"dup2()\": %s", strerror(errno));
		exit(1);
	}

	if (execlp("gzip", "gzip", "-c", NULL) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"execle()\": %s", strerror(errno));
		exit(1);
	}
	exit(1);
}

static int sus_compress_gzip(char *dat, int *dat_size)
{
	int tmp_fd, ret, wstatus;
	char tmp_path[32] = "/tmp/gzhttpXXXXXX.gz";
	process_t process;
	struct stat statbuf;

	tmp_fd = mkstemps(tmp_path, 3);
	if (tmp_fd == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"mkstemp()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (sus_create_process(&process, sus_compress_gzip_callback, &tmp_fd) == SUS_ERROR) {
		return SUS_ERROR;
	}

	ret = write(process.channel[0], dat, *dat_size);
	if (ret == -1 || ret != *dat_size) {
		sus_log_error(LEVEL_PANIC, "Failed \"write()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}
	if (close(process.channel[0]) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	/* TODO check return status of gzip if any error occured */
	if ((wstatus = sus_wait_process(&process)) == SUS_ERROR) {
		return SUS_ERROR;
	}

	if (fstat(tmp_fd, &statbuf) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"fstat()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (statbuf.st_size > *dat_size || statbuf.st_size <= 0) {
		sus_log_error(LEVEL_PANIC, "Wrong \"statbuf.st_size\": %d", statbuf.st_size);
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	if (lseek(tmp_fd, 0, SEEK_SET) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"lseek()\": %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}

	ret = read(tmp_fd, dat, statbuf.st_size);
	if (ret == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"read()\" tmp file: %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}
	*dat_size = ret;

	if (unlink(tmp_path) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"unlink()\" tmp_file: %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}
	if (close(tmp_fd) == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"close()\" tmp_file: %s", strerror(errno));
		sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
		return SUS_ERROR;
	}
	return SUS_OK;
}

int sus_compress_data(char *dat, int *dat_size, int compression_type)
{
	/* NOTE compress_data will overwrite dat and dat_size */
	switch (compression_type) {
		case GZIP:
			return sus_compress_gzip(dat, dat_size);
		case DEFLATE:
			sus_log_error(LEVEL_PANIC, "Unsupported deflate compression method");
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
		case BR:
			sus_log_error(LEVEL_PANIC, "Unsupported br compression method");
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
		default:
			sus_log_error(LEVEL_PANIC, "Unrecognized compression method: %d", compression_type);
			sus_set_errno(HTTP_INTERNAL_SERVER_ERROR);
			return SUS_ERROR;
	}
	return SUS_OK;
}
