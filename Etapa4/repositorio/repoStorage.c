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

// --- Copia el ROA de esa IP y su dueño ---
// Retorna STORAGE_OK o STORAGE_NOT_FOUND
int storageGet(const char *ip, char *roa, char *owner) {
    if (ip == NULL || roa ==  NULL || owner == NULL) {
        return STORAGE_NOT_FOUND;
    }

    // Buscar en el directorio con la ip
    DirEntry entry;
    if (fsFindDirEntry(ip, &entry, NULL) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Cargar los metadatos del inodo
    Inode inode;
    if (fsReadInode(entry.inodeIndex, &inode) != 0) {
        return STORAGE_NOT_FOUND;
    } else if (inode.isUsed != 1) {
        return STORAGE_NOT_FOUND;
    }

    // Cargar el contenido 
    uint8_t blockBuffer[FS_BLOCK_SIZE];
    if (fsReadBlock(inode.directBlocks[0], blockBuffer) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Copiar los datos hacia los parametros de salida dados
    snprintf(owner, OWNER_MAX_SIZE, "%s", inode.owner);
    snprintf(roa, ROA_MAX_SIZE, "%s", (char *)blockBuffer);

    printf("[FS] Consultado ROA: IP %s <- Inodo %u\n", ip, entry.inodeIndex);

    return STORAGE_OK;
}

// --- Guarda o reemplaza el ROA de esa IP y sube la versión --- 
// Retorna STORAGE_OK, STORAGE_FULL o STORAGE_NOT_FOUND
int storagePut(const char *ip, const char *owner, const char *roa) {
    if (ip == NULL || roa ==  NULL || owner == NULL) {
        return STORAGE_NOT_FOUND;
    }

    // Buscar en el directorio con la ip
    DirEntry entry;
    int inodeIdx = -1; 
    int blockIdx = -1;

    // Caso 1: Ya existe registro (Actualización)
    if (fsFindDirEntry(ip, &entry, NULL) == 0) {
        inodeIdx = (int)entry.inodeIndex;

        Inode inode;
        if (fsReadInode((uint32_t)inodeIdx, &inode) != 0) {
            return STORAGE_NOT_FOUND;
        }

        blockIdx = (int)inode.directBlocks[0];
        
    // Caso 2: Registro nuevo
    } else {
        inodeIdx = fsFindFreeInode();
        blockIdx = fsFindFreeBlock();

        if(inodeIdx < 0 || blockIdx < 0) {
            return STORAGE_FULL;
        }

        uint8_t blockBuffer[FS_BLOCK_SIZE];
        if (fsReadBlock(FS_DATA_BITMAP_BLOCK, blockBuffer) != 0) {
            return STORAGE_NOT_FOUND;
        }

        blockBuffer[blockIdx] = 1;

        if (fsWriteBlock(FS_DATA_BITMAP_BLOCK, blockBuffer) != 0) {
            return STORAGE_NOT_FOUND;
        }

        if (fsAddDirEntry(ip, (uint32_t)inodeIdx) != 0) {
            return STORAGE_FULL;
        }
    }

    // Escribir el ROA en el bloque de datos
    uint8_t dataBuffer[FS_BLOCK_SIZE];
    memset(dataBuffer, 0, FS_BLOCK_SIZE);
    strncpy((char *)dataBuffer, roa, FS_BLOCK_SIZE - 1);

    if (fsWriteBlock((uint32_t)blockIdx, dataBuffer) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Configurar y guardar el Inodo
    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.isUsed = 1;
    inode.directBlocks[0] = (uint32_t)blockIdx;
    inode.fileSize = (uint32_t)strlen(roa);
    strncpy(inode.owner, owner, sizeof(inode.owner) - 1);

    if (fsWriteInode((uint32_t)inodeIdx, &inode) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Actualizar la version en el Superbloque
    uint8_t superBlock[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_SUPERBLOCK_BLOCK, superBlock) != 0) {
        return STORAGE_NOT_FOUND;
    }

    SuperBlock *sb = (SuperBlock *)superBlock;
    sb->version++;
    if (fsWriteBlock(FS_SUPERBLOCK_BLOCK, superBlock) != 0) {
        return STORAGE_NOT_FOUND;
    }

    printf("[FS] Guardado ROA: IP %s -> Inodo %d, Bloque %d\n", ip, inodeIdx, blockIdx);
    return STORAGE_OK;
}