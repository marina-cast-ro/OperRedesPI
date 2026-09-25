#include "virtualMemory.h"
#include <string.h>

// Memoria física del enrutador: 32 marcos de 8 bytes.
// Es static para que ningún otro archivo la pueda tocar directo. 
// La única forma de llegar a un byte es por mmuReadByte y mmuWriteByte, que pasan por la MMU.
static uint8_t RAM[MEMORY_SIZE];

void virtualMemoryInit(void) {
    initMMU();
    memset(RAM, 0, sizeof(RAM));
}

int mmuWriteByte(uint32_t virtAddress, uint8_t data) {
    // translateAddress se queda solo con 5 bits de página, así que la dirección 256 daría la vuelta y caería encima de la página 0. Por eso el rango se valida aquí.
    if (virtAddress >= MEMORY_SIZE) {
        return -1;
    }

    uint32_t physAddress = translateAddress(virtAddress);
    if (physAddress == MMU_ERROR) {
        return -1;
    }

    RAM[physAddress] = data;
    return 0;
}

int mmuReadByte(uint32_t virtAddress, uint8_t *outData) {
    if (outData == NULL || virtAddress >= MEMORY_SIZE) {
        return -1;
    }

    uint32_t physAddress = translateAddress(virtAddress);
    if (physAddress == MMU_ERROR) {
        return -1;
    }

    *outData = RAM[physAddress];
    return 0;
}