#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "mmu.h"

#define BYTES_PER_PAGE 8                             // Tamaño de página que usa translateAddress (offset de 3 bits)
#define MEMORY_SIZE    (NUM_PAGES * BYTES_PER_PAGE)  // 32 páginas de 8 bytes = 256 bytes

// Inicializa el módulo de memoria y la RAM simulada
void virtualMemoryInit(void);

// Escribe un byte traduciendo la dirección virtual con la MMU
// Retorna 0 si pudo escribir, -1 si la dirección no es válida
int mmuWriteByte(uint32_t virtAddress, uint8_t data);

// Lee un byte traduciendo la dirección virtual con la MMU
// Retorna 0 si pudo leer (el byte queda en *outData), -1 si la dirección no es válida
int mmuReadByte(uint32_t virtAddress, uint8_t *outData);

#endif