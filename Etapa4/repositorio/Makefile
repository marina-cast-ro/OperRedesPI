#   make        compila el servidor en build/repoServer
#   make test   compila y corre las pruebas
#   make clean  borra la carpeta build/
# Necesita OpenSSL: sudo apt install libssl-dev
# Cuando esté el punto D, se cambia STORAGE por sus archivos

CC := gcc
BUILDDIR := build
CERTDIR := ../certificados
CFLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -pthread
LDLIBS := -lcrypto

STORAGE := repoStorageMemory.c
SOURCES := repoServer.c repoCommands.c $(STORAGE) $(CERTDIR)/certificates.c
HEADERS := repoCommands.h repoStorage.h $(CERTDIR)/certificates.h
SERVER := $(BUILDDIR)/repoServer

.PHONY: all test clean

all: $(SERVER)

$(SERVER): $(SOURCES) $(HEADERS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CPPFLAGS) -I$(CERTDIR) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)

test: $(SERVER)
	python3 testRepoServer.py

clean:
	rm -rf $(BUILDDIR)
