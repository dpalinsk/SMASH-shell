CC=gcc
CFLAGS= -g -std=c99 -pedantic -Wall -Wextra
all: smash

smash: smash.c
	$(CC) $(CFLAGS) -o smash smash.c -lreadline

clean:
	rm -f smash

