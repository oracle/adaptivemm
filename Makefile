CC=cc
CFLAGS=-I. -Wall -g
LDFLAGS=-lrt
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
	rm $(OBJS) memoptimizer

