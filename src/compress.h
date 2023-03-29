#ifndef COMPRESS_H
#define COMPRESS_H

#include "base.h"

#include "process.h"

#define GZIP    0
#define DEFLATE 1
#define BR      2

int compress_data(char *dat, int dat_size, int compression_type);

#endif
