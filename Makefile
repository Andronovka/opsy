CC = gcc
CFLAGS = -Wall -Wextra -pedantic -fPIC -g
LDFLAGS = -shared

all: libcaesar.so secure_copy

libcaesar.so: caesar.o
	$(CC) $(LDFLAGS) -o libcaesar.so caesar.o

caesar.o: caesar.c caesar.h
	$(CC) $(CFLAGS) -c caesar.c -o caesar.o

secure_copy: secure_copy.c caesar.h
	$(CC) -Wall -Wextra -pthread secure_copy.c -L. -lcaesar -o secure_copy -lrt -Wl,-rpath,.

clean:
	rm -f *.o *.so secure_copy log.txt
	rm -rf out/

.PHONY: all clean