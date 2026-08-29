# Incluir las cabeceras de include/
ccflags-y := -I$(src)/include

# Definir el módulo final y sus objetos dentro de src/
obj-m += kernelModule.o
kernelModule-objs := src/syscallModule.o src/stopAndWait.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M="$(PWD)" modules

clean:
	$(MAKE) -C $(KDIR) M="$(PWD)" clean