#ifndef COMPRESS_H
#define COMPRESS_H

#include "base.h"

#include "process.h"

#define GZIP    1
#define DEFLATE 2
#define BR      4

int compress_data(char *dat, int *dat_size, int compression_type);

#endif
