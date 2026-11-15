#include "repoStorage.h"
#include "fsDisk.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int setBitmapBit(uint32_t bitmapBlock, uint32_t index, uint8_t value) {
    if (index >= FS_BLOCK_SIZE) {
        return -1;
    }
    uint8_t buffer[FS_BLOCK_SIZE];
    if (fsReadBlock(bitmapBlock, buffer) != 0) {
        return -1;
    }
    buffer[index] = value;
    return fsWriteBlock(bitmapBlock, buffer);
}

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
    if (ip == NULL || roa == NULL || owner == NULL) {
        return STORAGE_NOT_FOUND;
    }

    DirEntry entry;
    int inodeIdx = -1;
    int blockIdx = -1;
    int isNew = 0;
    uint64_t createdAt = (uint64_t)time(NULL);
    uint64_t now = createdAt;

    // Caso 1: Ya existe registro (Actualización)
    if (fsFindDirEntry(ip, &entry, NULL) == 0) {
        inodeIdx = (int)entry.inodeIndex;

        Inode existingInode;
        if (fsReadInode((uint32_t)inodeIdx, &existingInode) != 0) {
            return STORAGE_NOT_FOUND;
        }
        if (existingInode.createdAt != 0) {
            createdAt = existingInode.createdAt;
        }

        blockIdx = (int)existingInode.directBlocks[0];
        uint8_t currentData[FS_BLOCK_SIZE];
        if (fsReadBlock((uint32_t)blockIdx, currentData) != 0) {
            return STORAGE_NOT_FOUND;
        }

        if (strcmp((char *)currentData, roa) == 0) {
            printf("[FS] ROA sin cambios: IP %s\n", ip);
            return STORAGE_OK;
        }
    } else {
        // Caso 2: Registro nuevo
        isNew = 1;
        inodeIdx = fsFindFreeInode();
        blockIdx = fsFindFreeBlock();
        if (inodeIdx < 0 || blockIdx < 0) {
            return STORAGE_FULL;
        }

        if (setBitmapBit(FS_DATA_BITMAP_BLOCK, (uint32_t)blockIdx, 1) != 0) {
            return STORAGE_NOT_FOUND;
        }
        if (setBitmapBit(FS_INODE_BITMAP_BLOCK, (uint32_t)inodeIdx, 1) != 0) {
            setBitmapBit(FS_DATA_BITMAP_BLOCK, (uint32_t)blockIdx, 0);
            return STORAGE_NOT_FOUND;
        }
    }

    // Escribir el ROA en el bloque de datos
    uint8_t dataBuffer[FS_BLOCK_SIZE];
    memset(dataBuffer, 0, FS_BLOCK_SIZE);
    strncpy((char *)dataBuffer, roa, FS_BLOCK_SIZE - 1);

    if (fsWriteBlock((uint32_t)blockIdx, dataBuffer) != 0) {
        if (isNew) {
            setBitmapBit(FS_INODE_BITMAP_BLOCK, (uint32_t)inodeIdx, 0);
            setBitmapBit(FS_DATA_BITMAP_BLOCK, (uint32_t)blockIdx, 0);
        }
        return STORAGE_NOT_FOUND;
    }

    // Configurar todos los campos y guardar el Inodo
    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.inodeID = (uint16_t)inodeIdx;
    inode.type = FS_TYPE_FILE;
    inode.isUsed = 1;
    inode.permissions = 0644;
    inode.fileSize = (uint32_t)strlen(roa);
    inode.createdAt = createdAt;
    inode.modifiedAt = now;
    strncpy(inode.owner, owner, sizeof(inode.owner) - 1);
    inode.directBlocks[0] = (uint32_t)blockIdx;
    inode.indirectBlock = 0;

    if (fsWriteInode((uint32_t)inodeIdx, &inode) != 0) {
        if (isNew) {
            setBitmapBit(FS_INODE_BITMAP_BLOCK, (uint32_t)inodeIdx, 0);
            setBitmapBit(FS_DATA_BITMAP_BLOCK, (uint32_t)blockIdx, 0);
        }
        return STORAGE_NOT_FOUND;
    }

    // Último paso de publicación para archivo nuevo: agregar entrada en directorio
    if (isNew && fsAddDirEntry(ip, (uint16_t)inodeIdx) != 0) {
        Inode zeroInode;
        memset(&zeroInode, 0, sizeof(Inode));
        fsWriteInode((uint32_t)inodeIdx, &zeroInode);
        setBitmapBit(FS_INODE_BITMAP_BLOCK, (uint32_t)inodeIdx, 0);
        setBitmapBit(FS_DATA_BITMAP_BLOCK, (uint32_t)blockIdx, 0);
        return STORAGE_FULL;
    }

    // Actualizar versión (y contadores si es archivo nuevo) en el Superbloque
    uint8_t superBlock[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_SUPERBLOCK_BLOCK, superBlock) != 0) {
        return STORAGE_NOT_FOUND;
    }

    SuperBlock *sb = (SuperBlock *)superBlock;
    sb->version++;
    if (isNew) {
        if (sb->freeBlocks > 0) {
            sb->freeBlocks--;
        }
        if (sb->freeInodes > 0) {
            sb->freeInodes--;
        }
    }
    if (fsWriteBlock(FS_SUPERBLOCK_BLOCK, superBlock) != 0) {
        return STORAGE_NOT_FOUND;
    }

    printf("[FS] Guardado ROA: IP %s -> Inodo %d, Bloque %d\n", ip, inodeIdx, blockIdx);
    return STORAGE_OK;
}

// --- Borra el ROA de esa IP y sube la versión ---
// Retorna STORAGE_OK o STORAGE_NOT_FOUND
int storageDelete(const char *ip) {
    if (ip == NULL) {
        return STORAGE_NOT_FOUND;
    }

    DirEntry entry;
    if (fsFindDirEntry(ip, &entry, NULL) != 0) {
        return STORAGE_NOT_FOUND;
    }

    Inode inode;
    if (fsReadInode(entry.inodeIndex, &inode) != 0) {
        return STORAGE_NOT_FOUND;
    }

    uint32_t blockIndex = inode.directBlocks[0];

    // Remover primero del directorio raíz antes de liberar inodo y bitmaps
    if (fsRemoveDirEntry(ip) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Limpiar el inodo
    memset(&inode, 0, sizeof(Inode));
    if (fsWriteInode(entry.inodeIndex, &inode) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Liberar bloque de datos e inodo en sus bitmaps
    if (setBitmapBit(FS_DATA_BITMAP_BLOCK, blockIndex, 0) != 0) {
        return STORAGE_NOT_FOUND;
    }
    if (setBitmapBit(FS_INODE_BITMAP_BLOCK, entry.inodeIndex, 0) != 0) {
        return STORAGE_NOT_FOUND;
    }

    // Actualizar versión y contadores libres del Superbloque
    uint8_t superBlock[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_SUPERBLOCK_BLOCK, superBlock) != 0) {
        return STORAGE_NOT_FOUND;
    }
    SuperBlock *sb = (SuperBlock *)superBlock;
    sb->version++;
    sb->freeBlocks++;
    sb->freeInodes++;
    if (fsWriteBlock(FS_SUPERBLOCK_BLOCK, superBlock) != 0) {
        return STORAGE_NOT_FOUND;
    }

    printf("[FS] Eliminado ROA: IP %s (Inodo %u liberado)\n", ip, entry.inodeIndex);
    return STORAGE_OK;
} 

// --- Copia todos los ROAs en el arreglo ---
// Retorna cuántos copió
size_t storageList(char roas[][ROA_MAX_SIZE], size_t maxRoas) {
    if (roas == NULL || maxRoas == 0) {
        return 0;
    }

    uint8_t buffer[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_DATA_BLOCKS_START, buffer) != 0) {
        return 0;
    }

    DirEntry *entries = (DirEntry *)buffer;

    size_t count = 0;

    for (int i = 0; i < 16 && count < maxRoas; i++) {
        if(entries[i].isUsed == 1) {
            Inode inode;
            if (fsReadInode(entries[i].inodeIndex, &inode) != 0) {
                continue;
            }

            if (inode.isUsed == 1) {
                uint8_t dataBuffer[FS_BLOCK_SIZE];
                if (fsReadBlock(inode.directBlocks[0], dataBuffer) == 0) {
                    snprintf(roas[count], ROA_MAX_SIZE, "%s", (char *)dataBuffer);
                    count++;
                }
            }

        }
    }

    printf("[FS] Listados %zu ROAs en el sistema\n", count);
    return count;
}