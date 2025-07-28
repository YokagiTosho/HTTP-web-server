CC=gcc
output=main

linked=../../src/request.c ../../src/config.c ../../src/log.c ../../src/sus_errno.c

src=$(linked) main.c
flags=-Wall -g -I../../src/

all:
	$(CC) $(flags) $(src) -o $(output)
