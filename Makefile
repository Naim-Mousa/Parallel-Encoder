CC=gcc
CFLAGS=-g
LDFLAGS = -pthread

.PHONY: all
all: nyuenc

nyuenc: nyuenc.o

nyuenc.o: nyuenc.c

.PHONY: clean
clean:
	rm -f *.o nyuenc
