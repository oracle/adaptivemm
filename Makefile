CC=gcc
CFLAGS=-I. -Wall -g
LDFLAGS=
OBJS=predict.o adaptivemmd.o

.DEFAULT_GOAL := adaptivemmd

all: adaptivemmd

predict.o: predict.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

adaptivemmd.o: adaptivemmd.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

adaptivemmd: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) adaptivemmd cscope.*

