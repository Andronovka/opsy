CC = gcc
CFLAGS = -Wall -Wextra -pedantic -fPIC
LDFLAGS = -shared

all: libcaesar.so secure_copy

libcaesar.so: caesar.o
	$(CC) $(LDFLAGS) -o libcaesar.so caesar.o

caesar.o: caesar.c caesar.h
	$(CC) $(CFLAGS) -c caesar.c -o caesar.o

secure_copy: secure_copy.c
	$(CC) -Wall -pthread secure_copy.c -L. -lcaesar -o secure_copy

install: libcaesar.so
	sudo cp libcaesar.so /usr/local/lib/
	sudo ldconfig

clean:
	rm -f *.o *.so secure_copy output.txt

.PHONY: all install clean