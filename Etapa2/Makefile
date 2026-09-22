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
SRCS := $(wildcard pi-router/src/*.c) $(wildcard src/*.c)
OBJS := $(patsubst %.c, $(BUILDDIR)/%.o, $(SRCS))

# La prueba solo necesita la MMU, la RAM y la tabla de rutas
TEST_SRCS := pi-router/src/mmu.c pi-router/src/virtualMemory.c pi-router/src/routingTable.c pi-router/tests/testMemory.c
TEST_OBJS := $(patsubst %.c, $(BUILDDIR)/%.o, $(TEST_SRCS))

DEPS := $(sort $(OBJS:.o=.d) $(TEST_OBJS:.o=.d))

# --- DEFINICIÓN DE TARGETS ---
TARGET := $(BUILDDIR)/router
TEST := $(BUILDDIR)/testMemory

# --- DEFINICIÓN DE BANDERAS DE COMPILACIÓN ---
CFLAGS := -Wall -Wextra
INCLUDES := -Ipi-router/include -Iinclude
LDFLAGS := -pthread

.PHONY: all run test clean

# --- TARGETS ---
all: $(TARGET)

# El router abre ../config.txt, por eso se corre desde pi-router/src
run: $(TARGET)
	cd pi-router/src && ../../$(TARGET)

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
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

# Inclusión de dependencias generadas automáticamente
-include $(DEPS)

# --- LIMPIEZA DE ARCHIVOS GENERADOS ---
clean:
	rm -rf $(BUILDDIR)
