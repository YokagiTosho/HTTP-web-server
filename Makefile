CC=gcc

flags=-Wall -g

src=src/*.c

out=webserver

all:
	$(CC) $(flags) $(src) -o $(out)
