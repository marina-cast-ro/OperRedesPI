# Makefile de la Etapa 2: compila todo el proyecto con un solo comando.
# Se usa desde la carpeta Etapa2/:
#   make        compila el router en build/router
#   make run    corre el router (desde pi-router/src, para que encuentre ../config.txt)
#   make test   compila y corre la prueba de la tabla de rutas
#   make clean  borra la carpeta build/

# --- DEFINICIÓN DE VARIABLES ---
CC := gcc
BUILDDIR := build

# Fuentes del router (pi-router/src) y del Módulo 1 de Sebas (src)
SRCS := main.c $(wildcard host/src/*.c) $(wildcard router/src/*.c)
OBJS := $(patsubst %.c, $(BUILDDIR)/%.o, $(SRCS))

# Prueba unitaria
TEST_SRCS := router/src/mmu.c router/src/virtualMemory.c router/src/routingTable.c router/tests/testMemory.c
TEST_OBJS := $(patsubst %.c, $(BUILDDIR)/%.o, $(TEST_SRCS))

DEPS := $(sort $(OBJS:.o=.d) $(TEST_OBJS:.o=.d))

# --- DEFINICIÓN DE TARGETS ---
TARGET := $(BUILDDIR)/routing
TEST := $(BUILDDIR)/testMemory

# --- BANDERAS DE COMPILACIÓN ---
CFLAGS := -Wall -Wextra -MMD
INCLUDES := -I. -Ihost/include -Irouter/include
LDFLAGS := -pthread

.PHONY: all run test clean

# --- TARGETS ---
all: $(TARGET)

# Ejecución desde host/src para conservar la ruta relativa del config
run: $(TARGET)
	./$(TARGET)

test: $(TEST)
	./$(TEST)

# Enlace de los ejecutables
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST): $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compilación de cada .c a su .o, dentro de build/ con la misma ruta de carpetas
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Inclusión de dependencias generadas automáticamente
-include $(DEPS)

# --- LIMPIEZA DE ARCHIVOS GENERADOS ---
clean:
	rm -rf $(BUILDDIR)
