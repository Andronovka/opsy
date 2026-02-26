CC = gcc
CFLAGS = -Wall -Wextra -pedantic -fPIC
LDFLAGS = -shared

all: libcaesar.so

libcaesar.so: caesar.o
	$(CC) $(LDFLAGS) -o libcaesar.so caesar.o

caesar.o: caesar.c caesar.h
	$(CC) $(CFLAGS) -c caesar.c -o caesar.o

install: libcaesar.so
	sudo cp libcaesar.so /usr/local/lib/
	sudo ldconfig

test: libcaesar.so input.txt
	python3 test.py ./libcaesar.so 3 input.txt output.txt

clean:
	rm -f *.o *.so output.txt

.PHONY: all install test clean