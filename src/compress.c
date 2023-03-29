#include "compress.h"

int compress_data(char *dat, int dat_size, int compression_type)
{
	/* NOTE compress_data will overwrite memory what dat points to with compressed data */
	switch (compression_type) {
		case GZIP:
			break;
		case DEFLATE:
			break;
		case BR:
			break;
		default:
			return SUS_ERROR;
	}
	return SUS_OK;
}
