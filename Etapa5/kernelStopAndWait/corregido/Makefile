# Makefile Kbuild para compilar el prototipo dentro del árbol del kernel (Etapa 1)
obj-y += kernel_corregido.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: modules check clean

# Compila solo el objeto: verifica que el código compila contra los headers del kernel
check:
	$(MAKE) -C $(KDIR) M=$(PWD) obj-m=kernel_corregido.o kernel_corregido.o

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) obj-m=kernel_corregido.o modules

clean:
	rm -f *.o *.ko *.mod* .*.cmd modules.order Module.symvers