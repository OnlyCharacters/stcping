CFLAGS = -g -Wall
CC = gcc

stcping: stcping.c
	$(CC) $(CFLAGS) $^ -o $@