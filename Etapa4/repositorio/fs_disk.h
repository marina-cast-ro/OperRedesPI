#ifndef FS_DISK_H
#define FS_DISK_H

#include <stdint.h>
#include <stddef.h>

// Fisico
#define DISK_PLATTERS 2
#define DISK_TRACKS_PER_PLATTER 16
#define DISK_SECTORS_PER_TRACK 8
#define DISK_SECTOR_SIZE 128
#define DISK_TOTAL_SECTORS (DISK_SECTORS_PER_TRACK * DISK_TRACKS_PER_PLATTER * DISK_PLATTERS)
#define DISK_SIZE_BYTES (DISK_TOTAL_SECTORS * DISK_SECTOR_SIZE)

// Logico
#define FS_BLOCK_SIZE 512
#define FS_TOTAL_BLOCKS (DISK_SIZE_BYTES / FS_BLOCK_SIZE)
#define FS_INODES 32
#define INODE_SIZE 128
#define DIR_ENTRY_SIZE 32
#define FS_MAGIC_NUMBER 0x524F4153

// Contiene la información de control global del sistema
typedef struct __attribute__((packed)) {
    // campos
    uint32_t magic_number; // numero magico para validar si el disco esta formateado
    uint64_t version;      // Contador incremental que pide storageVersion()
    uint32_t total_blocks; // Cantidad de bloques logicos del sistema de archivos
    uint32_t inode_count;  // Cantidad de inodos configurados
    uint32_t free_blocks; // Cantidad de bloques libres
    uint32_t free_inodes; // Cantidad de inodos libres
} SuperBlock;

typedef struct __attribute__((packed)) {
    // campos
    uint32_t is_used; // 1 si el inodo esta en uso, 0 si esta libre
    uint32_t file_size; // Tamaño del archivo en bytes
    uint32_t modified_at; // Timestamp de la ultima modificacion
    char owner[64]; // Nombre del propietario del archivo
    uint32_t direct_block; // Puntero al bloque de datos directo

} Inode;

typedef struct __attribute__((packed)) {
    // campos
    uint16_t inode_index; // Indice del inodo al que pertenece la entrada
    uint8_t is_used; // 1 si la entrada esta en uso, 0 si esta libre
    char ip[16]; // Direccion IP del propietario del archivo
    
} DirEntry;


#endif