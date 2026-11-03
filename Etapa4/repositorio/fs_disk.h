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

typedef struct __attribute__((packed)) {
    // campos
} SuperBlock;

typedef struct __attribute__((packed)) {
    // campos
} Inode;

typedef struct __attribute__((packed)) {
    // campos
} DirEntry;


#endif