#include "repoStorage.h"
#include "fsDisk.h"

#include <stdio.h>
#include <string.h>

// --- Prepara el disco ---
// Retorna STORAGE_OK, o -1 si no se pudo 
int storageInit(void) {
    uint8_t buffer[FS_BLOCK_SIZE];

    // Si no se puede leer el bloque, se formatea de cero y retorna
    if (fsReadBlock(FS_SUPERBLOCK_BLOCK, buffer) != 0) {
        if (fsFormat() != 0) {
            return -1;
        }
        return STORAGE_OK;
    }

    SuperBlock *superBlock = (SuperBlock *)buffer;

    // Si el bloque se leyó pero no tiene la firma válida, se formatea
    if (superBlock->magicNumber != FS_MAGIC_NUMBER) {
        if (fsFormat() != 0) {
            return -1;
        }
    }

    return STORAGE_OK;
}

// --- Contador de generación del superbloque ---
// Empieza en 0
long storageVersion(void) {
    uint8_t buffer[FS_BLOCK_SIZE];

    if (fsReadBlock(FS_SUPERBLOCK_BLOCK, buffer) != 0) {
        return -1; // VALOR DE ERROR
    }

    SuperBlock *superBlock = (SuperBlock *)buffer;

    return (long)superBlock->version;
}