#ifndef FS_DISK_H
#define FS_DISK_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Fisico
#define DISK_PLATTERS               2
#define DISK_TRACKS_PER_PLATTER     16
#define DISK_SECTORS_PER_TRACK      8
#define DISK_SECTOR_SIZE            128
#define DISK_TOTAL_SECTORS          (DISK_SECTORS_PER_TRACK * DISK_TRACKS_PER_PLATTER * DISK_PLATTERS)
#define DISK_SIZE_BYTES             (DISK_TOTAL_SECTORS * DISK_SECTOR_SIZE)

// Logico (Sistema de archivos)
#define FS_BLOCK_SIZE               512
#define FS_TOTAL_BLOCKS             (DISK_SIZE_BYTES / FS_BLOCK_SIZE)
#define FS_INODES                   32
#define INODE_SIZE                  128
#define DIR_ENTRY_SIZE              32
#define FS_MAGIC_NUMBER             0x524F4153

// Tipos de inodo
#define FS_TYPE_FREE                0
#define FS_TYPE_FILE                1
#define FS_TYPE_DIR                 2

// Offsets de bloques lógicos
#define FS_SUPERBLOCK_BLOCK         0
#define FS_INODE_BITMAP_BLOCK       1
#define FS_DATA_BITMAP_BLOCK        2
#define FS_INODE_TABLE_START        3
#define FS_INODE_TABLE_BLOCKS       8
#define FS_DATA_BLOCKS_START        11

// Estructura del Superbloque: Contiene la información de control global del sistema
typedef struct __attribute__((packed)) {
    // campos
    uint32_t magicNumber;          // Numero magico para validar si el disco esta formateado
    uint64_t version;               // Contador incremental que pide storageVersion()
    uint32_t totalBlocks;          // Cantidad de bloques logicos del sistema de archivos
    uint32_t inodeCount;           // Cantidad de inodos configurados
    uint32_t freeBlocks;           // Cantidad de bloques libres
    uint32_t freeInodes;           // Cantidad de inodos libres
    uint8_t  padding[480];          // Para completar los 512 bytes del bloque 0
} SuperBlock;

// Estructura del Inodo (Exactamente 128 bytes)
typedef struct __attribute__((packed)) {
    // campos
    uint16_t inodeID;
    uint8_t  type;                  // FS_TYPE_FILE o FS_TYPE_DIR
    uint32_t isUsed;               // 1 en uso, 0 libre
    uint16_t permissions;
    uint16_t reservedAlign;
    uint32_t fileSize;             // Tamaño del archivo en bytes
    uint64_t createdAt;            // Timestamp de la creacion
    uint64_t modifiedAt;           // Timestamp de la ultima modificacion
    char owner[64];                 // Nombre del propietario del archivo
    uint32_t directBlocks[4];      // 4 punteros directos exigidos por el diseño
    uint32_t indirectBlock;        // 1 puntero indirecto simple exigido
    uint8_t  padding[16];           // Completa 128 bytes exactos

} Inode;

// Entrada de Directorio (Exactamente 32 bytes)
typedef struct __attribute__((packed)) {
    // campos
    uint16_t inodeIndex;           // Indice del inodo al que pertenece la entrada
    uint8_t isUsed;                // 1 si la entrada esta en uso, 0 si esta libre  
    char ip[16];                    // Direccion IP del propietario del archivo
    uint8_t padding[13];            // Completa 32 bytes exactos
} DirEntry;

// --- FIRMAS --- 
// Traduccion fisica: Convierte plato, pista y sector a indice lineal LBA
uint32_t diskChsToLBA(uint32_t platter, uint32_t track, uint32_t sector);

// Operaciones de sector físico (128 bytes)
int diskReadSector(uint32_t lba, void *buffer);
int diskWriteSector(uint32_t lba, const void *buffer);

// Operaciones de bloque lógico (512 bytes = 4 sectores)
int fsReadBlock(uint32_t blockNum, void *buffer);
int fsWriteBlock(uint32_t blockNum, const void *buffer);

// Formateo / Inicialización del disco y superbloque
int fsFormat(void);

#endif