CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
TARGET  = fsDisk_ia
SRC     = fsDisk_ia.c

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) --test

clean:
	rm -f $(TARGET) *.o
