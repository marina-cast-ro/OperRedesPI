#include "fs_disk.h"
#include "repoStorage.h"

static uint8_t disk[DISK_SIZE_BYTES];

// Traduccion fisica: Convierte plato, pista y sector a indice lineal LBA
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

// Operaciones de sector físico (128 bytes)
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

// Operaciones de bloque lógico (512 bytes = 4 sectores)
int fsReadBlock(uint32_t blockNum, void *buffer) {
    // Validacion de existencia de buffer y limite de numero de bloque
    if (buffer == NULL || blockNum >= FS_TOTAL_BLOCKS) {
        return -1; // VALOR DE ERROR
    }

    uint32_t initialLBA = blockNum * 4;

    for(size_t i = 0; i < 4; ++i) {
        uint8_t* dest = ((uint8_t*)buffer) + (i * DISK_SECTOR_SIZE);
        if (diskReadSector(initialLBA + i, dest) != 0) {
            return -1;
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
        if (diskWriteSector(initialLBA + i, src) != 0) {
            return -1;
        }
    }

    return 0;
} 

int fsFormat(void) {
    memset(disk, 0, DISK_SIZE_BYTES);

    SuperBlock superBlock;
    memset(&superBlock, 0, sizeof(superBlock));

    superBlock.magicNumber = FS_MAGIC_NUMBER;
    superBlock.version = 0;
    superBlock.totalBlocks = FS_TOTAL_BLOCKS;
    superBlock.inodeCount  = FS_INODES;
    superBlock.freeBlocks = FS_TOTAL_BLOCKS - FS_DATA_BLOCKS_START; // 64 - 11 = 53
    superBlock.freeInodes = FS_INODES;

    uint8_t blockBuffer[FS_BLOCK_SIZE];
    memset(blockBuffer, 0, FS_BLOCK_SIZE);
    memcpy(blockBuffer, &superBlock, sizeof(SuperBlock));

    if (fsWriteBlock(0, blockBuffer) != 0) {
        return -1;
    }
    return 0;
}