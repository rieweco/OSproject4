#	MAKEFILE
CFLAGS = -g -Wall -Wshadow -o
GCC = gcc $(CFLAGS)

all: oss slave

oss: oss.c
	$(GCC) $(CFLAGS) oss oss.c

slave: slave.c
	$(GCC) $(CFLAGS) slave slave.c

clean:
	rm -f *.o oss slave
