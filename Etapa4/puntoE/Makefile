CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -I../validacionROAs

# Si esta instalado libssl-dev (/usr/include/openssl/x509.h) compila los 4 archivos;
# si no esta instalado todavia, compila roa_tabla.c y repo_cliente.c sin error.
ifneq ($(wildcard /usr/include/openssl/x509.h),)
    SRCS = repo_cliente.c roa_sync.c roa_tabla.c ../validacionROAs/roa_validate.c
    LIBS = -lcrypto -lpthread
else
    SRCS = repo_cliente.c roa_tabla.c
    LIBS = -lpthread
endif

TARGET = libpuntoE.so

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -shared $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
