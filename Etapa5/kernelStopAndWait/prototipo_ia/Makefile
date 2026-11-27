# Makefile Kbuild para compilar el prototipo dentro del árbol del kernel (Etapa 1)
obj-y += kernel_ia.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: modules clean

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) obj-m=kernel_ia.o modules

clean:
	rm -f *.o *.ko *.mod* .*.cmd modules.order Module.symvers
