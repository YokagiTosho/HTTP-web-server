#include "compress.h"

struct compress_data {
	char *data;
	int datalen;
	//char filepath[108];
	const char *filepath;
};

static void compress_gzip_callback(int fd, void *data)
{
	struct compress_data *_data = (struct compress_data *) data;

}

static int compress_gzip(char *dat, int *dat_size)
{
	struct compress_data data;

	data.data = dat;
	data.datalen = *dat_size;
	data.filepath = NULL; // TODO

	process_t process = create_process(compress_gzip_callback, &data);
	if (process.pid == INVALID_PID) {
		sus_log_error(LEVEL_PANIC, "Failed \"create_process()\"");
		return SUS_ERROR;
	}

	if (wait_process(&process) == SUS_ERROR) {
		sus_log_error(LEVEL_PANIC, "Failed \"wait_process()\"");
		return SUS_ERROR;
	}

	return SUS_OK;
}

int compress_data(char *dat, int *dat_size, int compression_type)
{
	/* NOTE compress_data will overwrite dat and dat_size */
	switch (compression_type) {
		case GZIP: return compress_gzip(dat, dat_size);
		case DEFLATE:
			sus_log_error(LEVEL_PANIC, "Unsupported deflate compression method");
			return SUS_ERROR;
			break;
		case BR:
			sus_log_error(LEVEL_PANIC, "Unsupported br compression method");
			return SUS_ERROR;
			break;
		default:
			sus_log_error(LEVEL_PANIC, "Unregonized compression method: %d", compression_type);
			return SUS_ERROR;
	}
	return SUS_OK;
}
