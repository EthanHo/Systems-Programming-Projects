CC = gcc
CFLAGS = -g -std=c99 -Wvla -Wall -fsanitize=address,undefined -pthread
LIB_FLAGS = -lm
DEPS = gen_queue.h

compare: compare.o gen_queue.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB_FLAGS)

ww: ww.o 
	$(CC) $(CFLAGS) -o $@ $^
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

test:
	./compare file1 README -s -d5 -f5 -a5
