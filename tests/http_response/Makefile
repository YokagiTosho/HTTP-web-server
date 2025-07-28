CC=gcc
output=main

linked=../../src/response.c ../../src/utils.c ../../src/compress.c ../../src/process.c ../../src/http.c ../../src/config.c ../../src/log.c ../../src/sus_errno.c ../../src/errors_html.c

src=$(linked) main.c
flags=-Wall -g -I../../src/

all:
	$(CC) $(flags) $(src) -o $(output)
