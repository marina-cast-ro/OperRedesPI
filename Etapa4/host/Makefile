CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lcrypto

TARGET = host_emisor
SRCS = host_emisor.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
