CFLAGS = -g -Wall
CC = gcc

.PHONY: all

all: stcping stcping_epoll

stcping: stcping.c
	$(CC) $(CFLAGS) $^ -o $@

stcping_epoll: stcping_epoll.c
	$(CC) $(CFLAGS) $^ -o $@

