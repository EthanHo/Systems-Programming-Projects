CC = gcc
CFLAGS = -g -std=gnu99 -Wvla -Wall -fsanitize=address,undefined -pthread
LIB_FLAGS = -lm
DEPS = packet_queue.h

server: server.o packet_queue.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB_FLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

test:
	./server 5722
