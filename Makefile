CC     = gcc
CFLAGS = -Wall -Wpedantic -Wextra -Werror

all: a1q1 a1procs a1threads

a1q1: a1q1.c
	$(CC) $(CFLAGS) -o a1q1 a1q1.c

a1procs: a1procs.c
	$(CC) $(CFLAGS) -o a1procs a1procs.c

a1threads: a1threads.c
	$(CC) $(CFLAGS) -o a1threads a1threads.c -lpthread

clean:
	rm -f a1q1 a1procs a1threads