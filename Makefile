CC = gcc

all: mine

run : mine
	./mine

mine: main.c
	$(CC) main.c -o mine -Wall -Wextra -pedantic -std=c99
