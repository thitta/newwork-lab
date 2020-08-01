CC = gcc
CFLAGS = -Wall -Wextra -Werror
STD=c99

main: main.c util.c util.h http.c http.h
	$(CC) main.c util.c http.c -o ./build/main $(CFLAGS) -std=$(STD)

.PHONY: clean
clean:
	rm ./build/*
