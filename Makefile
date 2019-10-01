CC=cc
CFLAGS=-I. -Wall -g
LDFLAGS=-lpthread
OBJS=predict.o predictord.o

.DEFAULT_GOAL := predictord

all: predictord

predict.o: predict.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

predictord.o: predictord.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

predictord: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm $(OBJS) predictord

