CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lcrypto

TARGETS = host_emisor host_receptor host_datos

all: $(TARGETS)

host_emisor: host_emisor.c
	$(CC) $(CFLAGS) host_emisor.c -o $@ $(LIBS)

host_receptor: host_receptor.c
	$(CC) $(CFLAGS) host_receptor.c -o $@

host_datos: host_datos.c
	$(CC) $(CFLAGS) host_datos.c -o $@

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
