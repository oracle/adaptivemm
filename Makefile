CC=gcc
CFLAGS=-I. -Wall -g
LDFLAGS=
OBJS=predict.o memoptimizer.o

.DEFAULT_GOAL := memoptimizer

all: memoptimizer

predict.o: predict.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

memoptimizer.o: memoptimizer.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

memoptimizer: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) memoptimizer cscope.*

