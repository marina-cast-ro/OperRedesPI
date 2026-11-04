# Makefile del punto B (formato y firma de ROA). 
# Se usa desde la carpeta del punto B
# 
#   make          compila build/roa_test (con sanitizers, para detectar fugas y errores de memoria)
#   make roa      firma un ROA nuevo en build/roa.txt con la llave y el certificado del nodo
#   make run      valida build/roa.txt  (debe imprimir OK)
#   make test     firma un ROA y corre todos los casos, validos y negativos (probar.sh)
#   make clean    borra build/
# Necesita libssl-dev (sudo apt install libssl-dev)
#
# Variables que se pueden cambiar:  make test IP=10.1.0.9 ISLA=65003 NUM=17 DIR=../prueba
#   DIR = carpeta con nodo/nodo-AS<ISLA>-<NUM>.key y .crt y truststore/trusted-roots.pem (la del punto A)

CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -Wshadow -g -fsanitize=address,undefined
LDLIBS  := -lcrypto
BUILD   := build
BIN     := $(BUILD)/roa_test

IP      ?= 10.1.0.5
ISLA    ?= 65003
NUM     ?= 17
DIR     ?= ../prueba
VALIDEZ ?= 86400

NODO    := $(DIR)/nodo/nodo-AS$(ISLA)-$(NUM)
ROOTS   := $(DIR)/truststore/trusted-roots.pem

.PHONY: all roa run test clean

all: $(BIN)

$(BIN): roa_validate.c test.c roa_validate.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) roa_validate.c test.c -o $@ $(LDLIBS)

# Firma un ROA nuevo (EMISION = ahora) y deja el certificado en base64 para el validador
roa: $(BUILD)/roa.txt
$(BUILD)/roa.txt: roa-firmar.sh $(NODO).key $(NODO).crt
	@mkdir -p $(BUILD)
	./roa-firmar.sh $(IP) $(ISLA) $(NUM) $(NODO).key $(NODO).crt $(VALIDEZ) > $@
	base64 -w0 $(NODO).crt > $(BUILD)/cert.b64
	@cat $@

# El ROA caduca: "make run" siempre firma uno nuevo antes de validar
run: $(BIN)
	@rm -f $(BUILD)/roa.txt
	@$(MAKE) --no-print-directory roa > /dev/null
	./$(BIN) $(ROOTS) $(BUILD)/cert.b64 $(BUILD)/roa.txt

test: $(BIN)
	@rm -f $(BUILD)/roa.txt
	@$(MAKE) --no-print-directory roa > /dev/null
	./probar.sh $(BIN) $(ROOTS) $(BUILD)/cert.b64 $(BUILD)/roa.txt $(NODO).key

clean:
	rm -rf $(BUILD)