CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -I../puntoE -I../validacionROAs

PUNTO_E_DIR = ../puntoE

# Si OpenSSL esta disponible, incluir el cliente y sincronizador completo
ifneq ($(wildcard /usr/include/openssl/x509.h),)
    SRCS = enrutador.c vector_distancia.c $(PUNTO_E_DIR)/roa_tabla.c $(PUNTO_E_DIR)/repo_cliente.c $(PUNTO_E_DIR)/roa_sync.c ../validacionROAs/roa_validate.c
    LIBS = -lpthread -lcrypto
else
    SRCS = enrutador.c vector_distancia.c $(PUNTO_E_DIR)/roa_tabla.c
    LIBS = -lpthread
endif

TARGET = enrutador

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
