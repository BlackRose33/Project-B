default: all

CC=gcc
CFLAGS=-Wall

all: proja

proja: router.c proxy.c router.h tunnel.c tunnel.h
	$(CC) $(CFLAGS) tunnel.c router.c proxy.c -o proja

clean:
	@echo "Cleaning for project A..."
	@rm -f proja *.o *.out