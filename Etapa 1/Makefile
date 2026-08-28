# --- DEFINICIÓN DE VARIABLES ---
CC ?= gcc
SRCDIR := src
INCDIR := include
BUILDDIR := build

# --- DEFINICIÓN DE TARGETS ---
TARGET_SERVER := $(BUILDDIR)/server
TARGET_CLIENT := $(BUILDDIR)/client
SERVER_OBJS := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(wildcard $(SRCDIR)/server*.c)) 
CLIENT_OBJS := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(wildcard $(SRCDIR)/client*.c))

# --- DEFINICIÓN DE ARCHIVOS FUENTE Y OBJETO ---
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

# --- DEFINICIÓN DE BANDERAS DE COMPILACIÓN ---
WARNFLAGS := -Wall -Wextra -Wpedantic -Wshadow
DEVFLAGS := -g3 -O0 -fsanitize=address,undefined $(WARNFLAGS)
TSANFLAGS := -g3 -O0 -fsanitize=thread $(WARNFLAGS)
OPTFLAGS := -O3 -march=native -funroll-loops $(WARNFLAGS)

# Banderas por defecto (si no se especifica otra) para compilación de desarrollo
CFLAGS ?= $(DEVFLAGS)
LDFLAGS := -pthread -lm

.PHONY: all server client dev tsan opt clean

# --- TARGETS ---
# General
all: $(TARGET_SERVER) $(TARGET_CLIENT)

# Compilaciones individuales por comando
server: $(TARGET_SERVER)
client: $(TARGET_CLIENT)

# Banderas para depuración de memoria con AdressSanitizer (solo compilación)
dev: clean
	$(MAKE) CFLAGS='$(DEVFLAGS)'

# Banderas para depuración de hilos con ThreadSanitizer (solo compilación)
tsan: clean
	$(MAKE) CC=clang CFLAGS='$(TSANFLAGS)'

# Banderas para optimización del código (solo compilación)
opt: clean
	$(MAKE) CFLAGS='$(OPTFLAGS)'

# Compilación de los archivos fuente y generación del ejecutable
$(TARGET_SERVER): $(SERVER_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_CLIENT): $(CLIENT_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compilación de archivos fuente individuales y generación de archivos objeto
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -MMD -c $< -o $@

# Creación del directorio de compilación si no existe
$(BUILDDIR):
	mkdir -p $@

# Inclusión de dependencias generadas automáticamente
-include $(DEPS)

# --- LIMPIEZA DE ARCHIVOS GENERADOS ---
clean:
	rm -rf $(BUILDDIR) 