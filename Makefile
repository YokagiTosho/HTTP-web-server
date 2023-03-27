CC=gcc

flags=-Wall

src=src/*.c

out=webserver

all:
	$(CC) $(flags) $(src) -o $(out)
