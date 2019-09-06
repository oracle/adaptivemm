CC=cc
CFLAGS=-I. -Wall
LDFLAGS=-lpthread
OBJS=predict.o predictord.o

.DEFAULT_GOAL := predictord

predict.o: predict.c predict_impl.h predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

predictord.o: predictord.c predict.h
	$(CC) -c -o $@ $< $(CFLAGS)

predictord: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm $(OBJS) predictord

