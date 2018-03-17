default: all

CC=gcc
CFLAGS=-Wall

all: projb

projb: router.c proxy.c router.h tunnel.c tunnel.h
	$(CC) $(CFLAGS) tunnel.c router.c proxy.c -o projb

clean:
	@echo "Cleaning for project B..."
	@rm -f projb *.o *.out
