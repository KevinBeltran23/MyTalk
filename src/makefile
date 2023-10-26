CC = gcc

CFLAGS = -Wall -pedantic -g

LD = gcc

LDFLAGS =

.SILENT:

all: mytalk

mytalk: talk.o
	$(LD) $(LDFLAGS) -L ~pn-cs357/Given/Talk/lib64 talk.o -o mytalk -ltalk -lncurses

talk.o: talk.c
	$(CC) $(CFLAGS) -I~pn-cs357/Given/Talk/include -c talk.c -o talk.o

test:
	valgrind --leak-check=yes ./talk

clean:
	rm -f *.o mytalk
