#include "fsDisk.h"
#include "repoStorage.h"

static uint8_t disk[DISK_SIZE_BYTES];

// --- Traduccion fisica: Convierte plato, pista y sector a indice lineal LBA ---
uint32_t diskChsToLBA(uint32_t platter, uint32_t track, uint32_t sector) {
    // Validacion de limites en numero de plato, pista y sector
    if (platter >= DISK_PLATTERS || 
        track >= DISK_TRACKS_PER_PLATTER || 
        sector >= DISK_SECTORS_PER_TRACK) {
        return UINT32_MAX; // VALOR DE ERROR 
    }
    
    uint32_t lba = (platter * DISK_TRACKS_PER_PLATTER * DISK_SECTORS_PER_TRACK) + 
    (track * DISK_SECTORS_PER_TRACK) + 
    sector;
    
    return lba;
}

// --- Operaciones de sector físico (128 bytes) ---
int diskReadSector(uint32_t lba, void *buffer) {
    // Validacion de existencia de buffer y limite de lba
    if (buffer == NULL || lba >= DISK_TOTAL_SECTORS) {
        return -1; // VALOR DE ERROR
    }

    uint32_t offset = lba * DISK_SECTOR_SIZE;

    memcpy(buffer, &disk[offset], DISK_SECTOR_SIZE);
    return 0; // VALOR DE EXITOR
}

int diskWriteSector(uint32_t lba, const void *buffer) {
    // Validacion de existencia de buffer y limite de lba
    if (buffer == NULL || lba >= DISK_TOTAL_SECTORS) {
        return -1; // VALOR DE ERROR
    }

    uint32_t offset = lba * DISK_SECTOR_SIZE;

    memcpy(&disk[offset], buffer, DISK_SECTOR_SIZE);
    return 0; // VALOR DE EXITOR
}

// --- Operaciones de bloque lógico (512 bytes = 4 sectores) ---
int fsReadBlock(uint32_t blockNum, void *buffer) {
    // Validacion de existencia de buffer y limite de numero de bloque
    if (buffer == NULL || blockNum >= FS_TOTAL_BLOCKS) {
        return -1; // VALOR DE ERROR
    }

    uint32_t initialLBA = blockNum * 4;

    for(size_t i = 0; i < 4; ++i) {
        uint8_t* dest = ((uint8_t*)buffer) + (i * DISK_SECTOR_SIZE);
        if (diskReadSector((uint32_t)(initialLBA + i), dest) != 0) {
            return -1; // VALOR DE ERROR
        }
    }

    return 0;
}

int fsWriteBlock(uint32_t blockNum, const void *buffer) {
    // Validacion de existencia de buffer y limite de numero de bloque
    if (buffer == NULL || blockNum >= FS_TOTAL_BLOCKS) {
        return -1; // VALOR DE ERROR
    }

    uint32_t initialLBA = blockNum * 4;

    for(size_t i = 0; i < 4; ++i) {
        const uint8_t* src = ((const uint8_t*)buffer) + (i * DISK_SECTOR_SIZE);
        if (diskWriteSector((uint32_t)(initialLBA + i), src) != 0) {
            return -1; // VALOR DE ERROR
        }
    }

    return 0;
} 

// --- Formateo / Inicialización del disco y superbloque ---
int fsFormat(void) {
    memset(disk, 0, DISK_SIZE_BYTES);

    SuperBlock superBlock;
    memset(&superBlock, 0, sizeof(superBlock));

    superBlock.magicNumber = FS_MAGIC_NUMBER;
    superBlock.version = 0;
    superBlock.totalBlocks = FS_TOTAL_BLOCKS;
    superBlock.inodeCount  = FS_INODES;
    superBlock.freeBlocks = FS_TOTAL_BLOCKS - (FS_DATA_BLOCKS_START + 1); // 64 - 12 = 52 (bloque 11 es dir raiz)
    superBlock.freeInodes = FS_INODES;

    uint8_t blockBuffer[FS_BLOCK_SIZE];
    memset(blockBuffer, 0, FS_BLOCK_SIZE);
    memcpy(blockBuffer, &superBlock, sizeof(SuperBlock));

    if (fsWriteBlock(FS_SUPERBLOCK_BLOCK, blockBuffer) != 0) {
        return -1;
    }

    // Marcar bloques del sistema (0..10) y directorio raiz (11) en el bitmap de datos
    uint8_t dataBitmap[FS_BLOCK_SIZE];
    memset(dataBitmap, 0, FS_BLOCK_SIZE);
    for (uint32_t b = 0; b <= FS_DATA_BLOCKS_START; b++) {
        dataBitmap[b] = 1;
    }
    if (fsWriteBlock(FS_DATA_BITMAP_BLOCK, dataBitmap) != 0) {
        return -1;
    }

    printf("[FS-DISK] Geometria inicializada: P=%d platos, T=%d pistas/plato, S=%d sectores/pista, B=%d bytes/sector (Total: %d KB, %d sectores LBA)\n",
           DISK_PLATTERS, DISK_TRACKS_PER_PLATTER, DISK_SECTORS_PER_TRACK, DISK_SECTOR_SIZE, DISK_SIZE_BYTES / 1024, DISK_TOTAL_SECTORS);
    printf("[FS] Formateo completado: 32 KB inicializados, Superbloque en bloque 0, %d inodos disponibles, %u bloques libres.\n",
           FS_INODES, superBlock.freeBlocks);
    fflush(stdout);
    return 0;
}

// --- Leer UN SOLO inode dentro de un bloque ---
int fsReadInode(uint32_t inodeIdx, Inode *inodeOut) {
    if (inodeOut == NULL || inodeIdx >= FS_INODES) {
        return -1; // VALOR DE ERROR
    }

    // Calculo de bloque
    uint32_t blockNum = FS_INODE_TABLE_START + (inodeIdx / 4);

    // Calculo del offset dentro del bloque
    size_t offset = (inodeIdx % 4) * sizeof(Inode);

    // Buffer para leer el bloque completo
    uint8_t blockBuffer[FS_BLOCK_SIZE];

    if (fsReadBlock(blockNum, blockBuffer) != 0) {
        return -1; // VALOR DE ERROR
    }

    memcpy(inodeOut, blockBuffer + offset, sizeof(Inode));

    return 0;
}

// --- Escribir en UN SOLO inode dentro de un bloque ---
int fsWriteInode(uint32_t inodeIdx, const Inode *inodeIn) {
    if (inodeIn == NULL || inodeIdx >= FS_INODES) {
        return -1; // VALOR DE ERROR
    }

    // Calculo de bloque
    uint32_t blockNum = FS_INODE_TABLE_START + (inodeIdx / 4);

    // Calculo del offset dentro del bloque
    size_t offset = (inodeIdx % 4) * sizeof(Inode);

    // Buffer para leer el bloque completo
    uint8_t blockBuffer[FS_BLOCK_SIZE];

    if (fsReadBlock(blockNum, blockBuffer) != 0) {
        return -1; // VALOR DE ERROR
    }

    memcpy(blockBuffer + offset, inodeIn, sizeof(Inode));

    if (fsWriteBlock(blockNum, blockBuffer) != 0) {
        return -1;
    }

    return 0;    
}

// --- Buscar un inodo libre ---
int fsFindFreeInode(void) {
    uint8_t bitmap[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_INODE_BITMAP_BLOCK, bitmap) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < FS_INODES; i++) {
        if (bitmap[i] != 0) {
            continue;
        }
        Inode inode;
        if (fsReadInode(i, &inode) == 0 && inode.isUsed == 0) {
            return (int)i; // Se ha encontrado un espacio disponible
        }
    }

    return -1; // VALOR DE ERROR: No se ha encontrado inodo libre
}

// --- Buscar un bloque de datos libre ---
int fsFindFreeBlock(void) {
    uint8_t bitmap[FS_BLOCK_SIZE];

    if (fsReadBlock(FS_DATA_BITMAP_BLOCK, bitmap) != 0) {
        return -1;
    }

    for(uint32_t b = FS_DATA_BLOCKS_START + 1; b < FS_TOTAL_BLOCKS; b++) {
        if (bitmap[b] == 0) {
            return (int)b; // Se ha encontrado un bloque libre
        }
    }

    return -1; // VALOR DE ERROR: No se ha encontrado bloque disponible
}

// --- Busca una IP en las entradas del directorio raiz (bloque 11) para obtener su inodo asociado ---
// Retorna 0 si existe o -1 si no se encuentra
int fsFindDirEntry(const char *ip, DirEntry *entryOut, uint32_t *entryIndexOut) {
    if(ip == NULL) {
        return -1; // VALOR DE ERROR
    }

    uint8_t buffer[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_DATA_BLOCKS_START, buffer) != 0) {
        return -1; // VALOR DE ERROR
    }

    DirEntry *entries = (DirEntry *)buffer;

    for(size_t i = 0; i < 16; i++) {
        if(entries[i].isUsed == 1 && 
            strncmp(entries[i].ip, ip, sizeof(entries[i].ip)) == 0) {
                if (entryOut != NULL) {
                    *entryOut = entries[i];
                }
                if (entryIndexOut != NULL) {
                    *entryIndexOut = (uint32_t)i;
                }

                return 0;
        }
    }

    return -1; // NO ENCONTRADA
}

// --- Busca una casilla libre en el directorio raiz (bloque 11) para añadir una nueva entrada ---
int fsAddDirEntry(const char *ip, uint16_t inodeIdx) {
    if (ip == NULL || inodeIdx >= FS_INODES) {
        return -1; // VALOR DE ERROR
    }

    uint8_t buffer[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_DATA_BLOCKS_START, buffer) != 0) {
        return -1; // VALOR DE ERROR
    }

    DirEntry *entries = (DirEntry *)buffer;

    for(size_t i = 0; i < 16; i++) {
        if(entries[i].isUsed == 0) {
            memset(&entries[i], 0, sizeof(DirEntry));
            
            entries[i].isUsed = 1;
            entries[i].inodeIndex = inodeIdx;
            strncpy(entries[i].ip, ip, sizeof(entries[i].ip) - 1);
            entries[i].ip[sizeof(entries[i].ip) - 1] = '\0'; // Asegurar terminador nulo

            if (fsWriteBlock(FS_DATA_BLOCKS_START, buffer) != 0) {
                return -1; // Falló la escritura física
            }

            return 0;
        }
    }

    return -1; // DIRECTORIO LLENO, NO CABEN MAS ARCHIVOS
}

// --- Borrar una entrada de directorio ---
int fsRemoveDirEntry(const char *ip) {
    if (ip == NULL) {
        return -1; // VALOR DE ERROR
    }

    uint8_t buffer[FS_BLOCK_SIZE];
    if (fsReadBlock(FS_DATA_BLOCKS_START, buffer) != 0) {
        return -1; // VALOR DE ERROR
    }

    DirEntry *entries = (DirEntry *)buffer;

    for(size_t i = 0; i < 16; i++) {
        if(entries[i].isUsed == 1 && 
            strncmp(entries[i].ip, ip, sizeof(entries[i].ip)) == 0) {
            memset(&entries[i], 0, sizeof(DirEntry));

            if (fsWriteBlock(FS_DATA_BLOCKS_START, buffer) != 0) {
                return -1; // Falló la escritura física
            }

            return 0;
        }
    }

    return -1; // La IP no estaba registrada en el directorio
}
