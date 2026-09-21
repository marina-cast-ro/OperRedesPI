#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "mmu.h"

// Inicializa el módulo de memoria y la RAM simulada
void virtualMemoryInit(void);

// Escribe un byte traduciendo la dirección virtual con la MMU
int mmuWriteByte(uint32_t virtAddress, uint8_t data);

// Lee un byte traduciendo la dirección virtual con la MMU
int mmuReadByte(uint32_t virtAddress, uint8_t *outData);

#endif 