# Makefile del punto A (certificados). Se usa desde Etapa4/certificados/:
#   make        compila la prueba en build/testCertificates
#   make clean  borra la carpeta build/
# Necesita la biblioteca de OpenSSL: sudo apt install libssl-dev

CC := gcc
BUILDDIR := build
CFLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
LDLIBS := -lcrypto

TEST := $(BUILDDIR)/testCertificates

.PHONY: all clean

all: $(TEST)

$(TEST): certificates.c testCertificates.c certificates.h
	@mkdir -p $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) certificates.c testCertificates.c -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(BUILDDIR)
