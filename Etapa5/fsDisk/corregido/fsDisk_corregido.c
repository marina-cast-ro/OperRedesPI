/*
 * fsDisk_corregido.c (versión de fsDisk_ia.c con 2 errores corregidos)
 *
 * Simulador de disco en memoria de 32 KB con geometría CHS y sistema de
 * archivos por bloques de 512 B para gestión de ROAs (Route Origin
 * Authorizations), utilizando únicamente la biblioteca estándar de C.
 *
 * Geometría física (32 KB = 32768 B):
 *   - 2 platos (H: 0..1)
 *   - 16 pistas/cilindros por plato (C: 0..15)
 *   - 8 sectores por pista (S: 0..7)
 *   - 128 bytes por sector => 256 sectores LBA (0..255)
 *
 * Capa lógica (64 bloques de 512 B = 4 sectores por bloque):
 *   - Bloque 0:     Superbloque
 *   - Bloque 1:     Bitmap de inodos (32 bits activos)
 *   - Bloque 2:     Bitmap de bloques de datos (52 bits activos: bloques 12..63)
 *   - Bloques 3-10: Tabla de 32 inodos (128 B cada uno, 4 por bloque)
 *   - Bloque 11:    Directorio raíz (16 entradas de 32 B: IP -> inodeIndex)
 *   - Bloques 12-63: Bloques de datos (52 bloques)
 */

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================== */
/* Constantes de geometría física y distribución lógica                       */
/* ========================================================================== */

#define DISK_PLATTERS            2
#define DISK_TRACKS_PER_PLATTER  16
#define DISK_SECTORS_PER_TRACK   8
#define SECTOR_SIZE              128
#define TOTAL_SECTORS            (DISK_PLATTERS * DISK_TRACKS_PER_PLATTER * DISK_SECTORS_PER_TRACK) /* 256 */
#define DISK_SIZE_BYTES          (TOTAL_SECTORS * SECTOR_SIZE)                                      /* 32768 */

#define SECTORS_PER_BLOCK        4
#define BLOCK_SIZE               (SECTORS_PER_BLOCK * SECTOR_SIZE)                                  /* 512 */
#define TOTAL_BLOCKS             (TOTAL_SECTORS / SECTORS_PER_BLOCK)                                /* 64 */

#define SUPERBLOCK_BLOCK         0
#define INODE_BITMAP_BLOCK       1
#define DATA_BITMAP_BLOCK        2
#define INODE_TABLE_START        3
#define INODE_TABLE_BLOCKS       8                                                                  /* Bloques 3..10 */
#define ROOT_DIR_BLOCK           11
#define DATA_BLOCKS_START        12
#define DATA_BLOCKS_END          63
#define TOTAL_DATA_BLOCKS        (TOTAL_BLOCKS - DATA_BLOCKS_START)                                 /* 52 */

#define INODE_SIZE               128
#define INODES_PER_BLOCK         (BLOCK_SIZE / INODE_SIZE)                                          /* 4 */
#define TOTAL_INODES             (INODE_TABLE_BLOCKS * INODES_PER_BLOCK)                            /* 32 */
#define INODE_DIRECT_PTRS        4
#define INDIRECT_PTRS_PER_BLOCK  (BLOCK_SIZE / (int)sizeof(int32_t))                                /* 128 */

#define DIR_ENTRY_SIZE           32
#define ROOT_DIR_MAX_ENTRIES     (BLOCK_SIZE / DIR_ENTRY_SIZE)                                      /* 16 */
#define MAX_IP_LEN               23  /* 24 bytes incluyendo '\0' */
#define MAX_OWNER_LEN            31  /* 32 bytes incluyendo '\0' */
#define MAX_ROA_SIZE             512

#define FS_MAGIC                 0x4653444Bu /* "FSDK" */
#define INVALID_BLOCK            (-1)
#define INVALID_INODE            (-1)

/* ========================================================================== */
/* Códigos de error                                                           */
/* ========================================================================== */

typedef enum {
    FS_OK                   = 0,
    FS_ERR_INVALID_ARG      = -1,
    FS_ERR_OUT_OF_RANGE     = -2,
    FS_ERR_NOT_FORMATTED    = -3,
    FS_ERR_NO_SPACE         = -4,
    FS_ERR_NO_INODES        = -5,
    FS_ERR_DIR_FULL         = -6,
    FS_ERR_NOT_FOUND        = -7,
    FS_ERR_TOO_LARGE        = -8,
    FS_ERR_BUFFER_TOO_SMALL = -9,
    FS_ERR_CORRUPT          = -10
} FsError;

static const char *fs_strerror(int err) {
    switch (err) {
        case FS_OK:                   return "Operación exitosa";
        case FS_ERR_INVALID_ARG:      return "Argumento inválido";
        case FS_ERR_OUT_OF_RANGE:     return "Dirección o índice fuera de rango";
        case FS_ERR_NOT_FORMATTED:    return "El disco no está formateado (número mágico inválido)";
        case FS_ERR_NO_SPACE:         return "No hay bloques de datos libres suficientes";
        case FS_ERR_NO_INODES:        return "No hay inodos libres disponibles";
        case FS_ERR_DIR_FULL:         return "El directorio raíz está lleno (máximo 16 entradas)";
        case FS_ERR_NOT_FOUND:        return "ROA/IP no encontrado";
        case FS_ERR_TOO_LARGE:        return "El tamaño excede el límite permitido (máximo 512 B para ROA)";
        case FS_ERR_BUFFER_TOO_SMALL: return "El búfer de destino es demasiado pequeño";
        case FS_ERR_CORRUPT:          return "Inconsistencia detectada en las estructuras del disco";
        default:                      return "Error desconocido";
    }
}

/* ========================================================================== */
/* Estructuras en disco                                                       */
/* ========================================================================== */

/* Bloque 0: Superbloque (exactamente 512 bytes) */
typedef struct {
    uint32_t magic;             /* Número mágico para verificar inicialización */
    uint32_t version;           /* Contador de versión (incrementa con put/delete) */
    uint32_t total_blocks;      /* Bloques totales del disco (64) */
    uint32_t data_blocks;       /* Bloques totales de datos (52) */
    uint32_t free_blocks;       /* Bloques de datos libres (0..52) */
    uint32_t total_inodes;      /* Inodos totales (32) */
    uint32_t free_inodes;       /* Inodos libres (0..32) */
    uint32_t block_size;        /* Bytes por bloque (512) */
    uint32_t sector_size;       /* Bytes por sector (128) */
    uint32_t first_data_block;  /* Primer bloque de datos (12) */
    uint32_t inode_table_start; /* Primer bloque de la tabla de inodos (3) */
    uint32_t root_dir_block;    /* Bloque del directorio raíz (11) */
    uint8_t  reserved[BLOCK_SIZE - 12u * sizeof(uint32_t)];
} Superblock;

/* Bloques 3-10: Inodo (exactamente 128 bytes) */
typedef struct {
    uint8_t  in_use;                    /* 0 = libre, 1 = ocupado */
    uint8_t  type;                      /* 1 = ROA / archivo regular */
    uint16_t permissions;               /* Permisos simbólicos (ej. 0644) */
    uint32_t size;                      /* Tamaño de datos en bytes */
    int32_t  direct[INODE_DIRECT_PTRS]; /* 4 punteros directos a bloques (-1 = libre) */
    int32_t  indirect;                  /* 1 puntero indirecto simple (-1 = libre) */
    uint32_t block_count;               /* Total de bloques físicos asignados */
    int64_t  created_at;                /* Timestamp de creación (epoch seg) */
    int64_t  modified_at;               /* Timestamp de última modificación */
    int64_t  accessed_at;               /* Timestamp de último acceso */
    char     owner[MAX_OWNER_LEN + 1];  /* Dueño (cadena terminada en '\0') */
    uint8_t  padding[40];               /* Relleno para completar 128 bytes */
} Inode;

/* Bloque 11: Entrada del directorio raíz (exactamente 32 bytes) */
typedef struct {
    char    ip[MAX_IP_LEN + 1]; /* IP o prefijo IP (terminado en '\0') */
    int32_t inode_index;        /* Índice de inodo (0..31), -1 si libre */
    uint8_t in_use;             /* 0 = libre, 1 = ocupada */
    uint8_t reserved[3];        /* Alineación a 32 bytes */
} DirEntry;

/* Disco simulado en memoria: exactamente 32 KB (2 x 16 x 8 x 128) */
typedef struct {
    uint8_t storage[DISK_PLATTERS][DISK_TRACKS_PER_PLATTER][DISK_SECTORS_PER_TRACK][SECTOR_SIZE];
} Disk;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(Superblock) == BLOCK_SIZE, "Superblock debe medir exactamente 512 bytes");
_Static_assert(sizeof(Inode) == INODE_SIZE, "Inode debe medir exactamente 128 bytes");
_Static_assert(sizeof(DirEntry) == DIR_ENTRY_SIZE, "DirEntry debe medir exactamente 32 bytes");
_Static_assert(ROOT_DIR_MAX_ENTRIES * sizeof(DirEntry) == BLOCK_SIZE, "Directorio raiz debe ocupar 512 bytes");
_Static_assert(sizeof(Disk) == DISK_SIZE_BYTES, "Disk debe medir exactamente 32768 bytes (32 KB)");
#endif

/* ========================================================================== */
/* Utilidades internas                                                        */
/* ========================================================================== */

static void safe_copy_str(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/*
 * CORREGIDO (error 1): la versión de la IA aceptaba cualquier cadena imprimible
 * ("hola", "999.999.999.999/99"). Ahora se exige IPv4 con formato a.b.c.d o
 * a.b.c.d/n, con cada octeto en 0..255 y el prefijo n en 0..32.
 */
static bool is_valid_ip_key(const char *ip) {
    if (!ip) {
        return false;
    }
    size_t len = strlen(ip);
    if (len == 0 || len > MAX_IP_LEN) {
        return false;
    }
    const char *p = ip;
    for (int octet = 0; octet < 4; ++octet) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
        int val = 0, digits = 0;
        while (isdigit((unsigned char)*p)) {
            val = val * 10 + (*p - '0');
            if (++digits > 3 || val > 255) {
                return false;
            }
            ++p;
        }
        if (octet < 3) {
            if (*p != '.') {
                return false;
            }
            ++p;
        }
    }
    if (*p == '/') {
        ++p;
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
        int prefix = 0, digits = 0;
        while (isdigit((unsigned char)*p)) {
            prefix = prefix * 10 + (*p - '0');
            if (++digits > 2 || prefix > 32) {
                return false;
            }
            ++p;
        }
    }
    return *p == '\0';
}

static void format_timestamp(int64_t ts, char *buf, size_t buf_size) {
    if (ts <= 0) {
        safe_copy_str(buf, buf_size, "N/A");
        return;
    }
    time_t t = (time_t)ts;
    struct tm *tm_info = localtime(&t);
    if (!tm_info) {
        snprintf(buf, buf_size, "%" PRId64, ts);
        return;
    }
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* ========================================================================== */
/* Capa física (CHS / LBA / Sectores) y capa lógica (Bloques)                 */
/* ========================================================================== */

static void disk_init(Disk *disk) {
    if (!disk) {
        return;
    }
    memset(disk, 0, sizeof(*disk));
}

/*
 * Traducción CHS -> LBA (todos los índices base 0):
 *   cylinder (pista): 0..15
 *   head     (plato): 0..1
 *   sector  (sector): 0..7
 * Fórmula estándar: LBA = (C * PLATTERS + H) * SECTORS_PER_TRACK + S
 */
static int chs_to_lba(int cylinder, int head, int sector) {
    if (cylinder < 0 || cylinder >= DISK_TRACKS_PER_PLATTER ||
        head < 0 || head >= DISK_PLATTERS ||
        sector < 0 || sector >= DISK_SECTORS_PER_TRACK) {
        return FS_ERR_OUT_OF_RANGE;
    }
    return (cylinder * DISK_PLATTERS + head) * DISK_SECTORS_PER_TRACK + sector;
}

/* Traducción inversa LBA -> CHS */
static int lba_to_chs(int lba, int *cylinder, int *head, int *sector) {
    if (!cylinder || !head || !sector) {
        return FS_ERR_INVALID_ARG;
    }
    if (lba < 0 || lba >= TOTAL_SECTORS) {
        return FS_ERR_OUT_OF_RANGE;
    }
    int sectors_per_cylinder = DISK_PLATTERS * DISK_SECTORS_PER_TRACK;
    *cylinder = lba / sectors_per_cylinder;
    int rem = lba % sectors_per_cylinder;
    *head = rem / DISK_SECTORS_PER_TRACK;
    *sector = rem % DISK_SECTORS_PER_TRACK;
    return FS_OK;
}

static int read_sector_lba(const Disk *disk, int lba, uint8_t buffer[SECTOR_SIZE]) {
    if (!disk || !buffer) {
        return FS_ERR_INVALID_ARG;
    }
    int c = 0, h = 0, s = 0;
    int rc = lba_to_chs(lba, &c, &h, &s);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(buffer, disk->storage[h][c][s], SECTOR_SIZE);
    return FS_OK;
}

static int write_sector_lba(Disk *disk, int lba, const uint8_t buffer[SECTOR_SIZE]) {
    if (!disk || !buffer) {
        return FS_ERR_INVALID_ARG;
    }
    int c = 0, h = 0, s = 0;
    int rc = lba_to_chs(lba, &c, &h, &s);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(disk->storage[h][c][s], buffer, SECTOR_SIZE);
    return FS_OK;
}

static int read_sector_chs(const Disk *disk, int cylinder, int head, int sector,
                           uint8_t buffer[SECTOR_SIZE]) {
    int lba = chs_to_lba(cylinder, head, sector);
    if (lba < 0) {
        return lba;
    }
    return read_sector_lba(disk, lba, buffer);
}

static int write_sector_chs(Disk *disk, int cylinder, int head, int sector,
                            const uint8_t buffer[SECTOR_SIZE]) {
    int lba = chs_to_lba(cylinder, head, sector);
    if (lba < 0) {
        return lba;
    }
    return write_sector_lba(disk, lba, buffer);
}

static int read_block(const Disk *disk, int block_num, uint8_t buffer[BLOCK_SIZE]) {
    if (!disk || !buffer) {
        return FS_ERR_INVALID_ARG;
    }
    if (block_num < 0 || block_num >= TOTAL_BLOCKS) {
        return FS_ERR_OUT_OF_RANGE;
    }
    int base_lba = block_num * SECTORS_PER_BLOCK;
    for (int i = 0; i < SECTORS_PER_BLOCK; ++i) {
        int rc = read_sector_lba(disk, base_lba + i, buffer + (size_t)i * SECTOR_SIZE);
        if (rc != FS_OK) {
            return rc;
        }
    }
    return FS_OK;
}

static int write_block(Disk *disk, int block_num, const uint8_t buffer[BLOCK_SIZE]) {
    if (!disk || !buffer) {
        return FS_ERR_INVALID_ARG;
    }
    if (block_num < 0 || block_num >= TOTAL_BLOCKS) {
        return FS_ERR_OUT_OF_RANGE;
    }
    int base_lba = block_num * SECTORS_PER_BLOCK;
    for (int i = 0; i < SECTORS_PER_BLOCK; ++i) {
        int rc = write_sector_lba(disk, base_lba + i, buffer + (size_t)i * SECTOR_SIZE);
        if (rc != FS_OK) {
            return rc;
        }
    }
    return FS_OK;
}

/* ========================================================================== */
/* Gestión de superbloque, bitmaps, tabla de inodos y bloques de datos        */
/* ========================================================================== */

static bool bitmap_test(const uint8_t bitmap[BLOCK_SIZE], int bit_index) {
    return (bitmap[bit_index / 8] & (uint8_t)(1u << (bit_index % 8))) != 0;
}

static void bitmap_set(uint8_t bitmap[BLOCK_SIZE], int bit_index) {
    bitmap[bit_index / 8] |= (uint8_t)(1u << (bit_index % 8));
}

static void bitmap_clear(uint8_t bitmap[BLOCK_SIZE], int bit_index) {
    bitmap[bit_index / 8] &= (uint8_t)~(1u << (bit_index % 8));
}

static int fs_read_superblock(const Disk *disk, Superblock *sb) {
    if (!disk || !sb) {
        return FS_ERR_INVALID_ARG;
    }
    uint8_t raw[BLOCK_SIZE];
    int rc = read_block(disk, SUPERBLOCK_BLOCK, raw);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(sb, raw, sizeof(Superblock));
    if (sb->magic != FS_MAGIC ||
        sb->total_blocks != TOTAL_BLOCKS ||
        sb->data_blocks != TOTAL_DATA_BLOCKS ||
        sb->total_inodes != TOTAL_INODES ||
        sb->free_blocks > TOTAL_DATA_BLOCKS ||
        sb->free_inodes > TOTAL_INODES) {
        return FS_ERR_NOT_FORMATTED;
    }
    return FS_OK;
}

static int fs_write_superblock(Disk *disk, const Superblock *sb) {
    if (!disk || !sb) {
        return FS_ERR_INVALID_ARG;
    }
    uint8_t raw[BLOCK_SIZE];
    memcpy(raw, sb, sizeof(Superblock));
    return write_block(disk, SUPERBLOCK_BLOCK, raw);
}

static bool fs_is_formatted(const Disk *disk) {
    Superblock sb;
    return fs_read_superblock(disk, &sb) == FS_OK;
}

static void inode_reset_empty(Inode *inode) {
    memset(inode, 0, sizeof(*inode));
    for (int i = 0; i < INODE_DIRECT_PTRS; ++i) {
        inode->direct[i] = INVALID_BLOCK;
    }
    inode->indirect = INVALID_BLOCK;
}

static int fs_format(Disk *disk) {
    if (!disk) {
        return FS_ERR_INVALID_ARG;
    }

    uint8_t zero_block[BLOCK_SIZE];
    memset(zero_block, 0, sizeof(zero_block));

    /* Limpiar todos los bloques a través de la capa lógica */
    for (int b = 0; b < TOTAL_BLOCKS; ++b) {
        int rc = write_block(disk, b, zero_block);
        if (rc != FS_OK) {
            return rc;
        }
    }

    /* Bloque 0: Superbloque */
    Superblock sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic             = FS_MAGIC;
    sb.version           = 0;
    sb.total_blocks      = TOTAL_BLOCKS;
    sb.data_blocks       = TOTAL_DATA_BLOCKS;
    sb.free_blocks       = TOTAL_DATA_BLOCKS;
    sb.total_inodes      = TOTAL_INODES;
    sb.free_inodes       = TOTAL_INODES;
    sb.block_size        = BLOCK_SIZE;
    sb.sector_size       = SECTOR_SIZE;
    sb.first_data_block  = DATA_BLOCKS_START;
    sb.inode_table_start = INODE_TABLE_START;
    sb.root_dir_block    = ROOT_DIR_BLOCK;

    int rc = fs_write_superblock(disk, &sb);
    if (rc != FS_OK) {
        return rc;
    }

    /* Bloques 3-10: Tabla de 32 inodos inicializados con punteros en -1 */
    Inode empty_inodes[INODES_PER_BLOCK];
    for (int i = 0; i < INODES_PER_BLOCK; ++i) {
        inode_reset_empty(&empty_inodes[i]);
    }
    uint8_t raw_inode_block[BLOCK_SIZE];
    memcpy(raw_inode_block, empty_inodes, sizeof(raw_inode_block));
    for (int b = 0; b < INODE_TABLE_BLOCKS; ++b) {
        rc = write_block(disk, INODE_TABLE_START + b, raw_inode_block);
        if (rc != FS_OK) {
            return rc;
        }
    }

    /* Bloque 11: Directorio raíz con 16 entradas vacías */
    DirEntry empty_dir[ROOT_DIR_MAX_ENTRIES];
    memset(empty_dir, 0, sizeof(empty_dir));
    for (int i = 0; i < ROOT_DIR_MAX_ENTRIES; ++i) {
        empty_dir[i].inode_index = INVALID_INODE;
        empty_dir[i].in_use = 0;
    }
    uint8_t raw_dir_block[BLOCK_SIZE];
    memcpy(raw_dir_block, empty_dir, sizeof(raw_dir_block));
    rc = write_block(disk, ROOT_DIR_BLOCK, raw_dir_block);
    if (rc != FS_OK) {
        return rc;
    }

    return FS_OK;
}

static int fs_read_inode(const Disk *disk, int inode_idx, Inode *inode) {
    if (!disk || !inode) {
        return FS_ERR_INVALID_ARG;
    }
    if (inode_idx < 0 || inode_idx >= TOTAL_INODES) {
        return FS_ERR_OUT_OF_RANGE;
    }
    int block_num = INODE_TABLE_START + (inode_idx / INODES_PER_BLOCK);
    int slot      = inode_idx % INODES_PER_BLOCK;
    uint8_t raw[BLOCK_SIZE];
    int rc = read_block(disk, block_num, raw);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(inode, raw + (size_t)slot * INODE_SIZE, sizeof(Inode));
    return FS_OK;
}

static int fs_write_inode(Disk *disk, int inode_idx, const Inode *inode) {
    if (!disk || !inode) {
        return FS_ERR_INVALID_ARG;
    }
    if (inode_idx < 0 || inode_idx >= TOTAL_INODES) {
        return FS_ERR_OUT_OF_RANGE;
    }
    int block_num = INODE_TABLE_START + (inode_idx / INODES_PER_BLOCK);
    int slot      = inode_idx % INODES_PER_BLOCK;
    uint8_t raw[BLOCK_SIZE];
    int rc = read_block(disk, block_num, raw);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(raw + (size_t)slot * INODE_SIZE, inode, sizeof(Inode));
    return write_block(disk, block_num, raw);
}

static int fs_alloc_inode(Disk *disk, Superblock *sb) {
    if (sb->free_inodes == 0) {
        return FS_ERR_NO_INODES;
    }
    uint8_t bitmap[BLOCK_SIZE];
    int rc = read_block(disk, INODE_BITMAP_BLOCK, bitmap);
    if (rc != FS_OK) {
        return rc;
    }
    for (int i = 0; i < TOTAL_INODES; ++i) {
        if (!bitmap_test(bitmap, i)) {
            bitmap_set(bitmap, i);
            rc = write_block(disk, INODE_BITMAP_BLOCK, bitmap);
            if (rc != FS_OK) {
                return rc;
            }
            sb->free_inodes--;
            return i;
        }
    }
    return FS_ERR_NO_INODES;
}

static int fs_free_inode(Disk *disk, Superblock *sb, int inode_idx) {
    if (inode_idx < 0 || inode_idx >= TOTAL_INODES) {
        return FS_ERR_OUT_OF_RANGE;
    }
    uint8_t bitmap[BLOCK_SIZE];
    int rc = read_block(disk, INODE_BITMAP_BLOCK, bitmap);
    if (rc != FS_OK) {
        return rc;
    }
    if (bitmap_test(bitmap, inode_idx)) {
        bitmap_clear(bitmap, inode_idx);
        rc = write_block(disk, INODE_BITMAP_BLOCK, bitmap);
        if (rc != FS_OK) {
            return rc;
        }
        if (sb->free_inodes < TOTAL_INODES) {
            sb->free_inodes++;
        }
    }
    return FS_OK;
}

static int fs_alloc_data_block(Disk *disk, Superblock *sb) {
    if (sb->free_blocks == 0) {
        return FS_ERR_NO_SPACE;
    }
    uint8_t bitmap[BLOCK_SIZE];
    int rc = read_block(disk, DATA_BITMAP_BLOCK, bitmap);
    if (rc != FS_OK) {
        return rc;
    }
    for (int i = 0; i < TOTAL_DATA_BLOCKS; ++i) {
        if (!bitmap_test(bitmap, i)) {
            bitmap_set(bitmap, i);
            rc = write_block(disk, DATA_BITMAP_BLOCK, bitmap);
            if (rc != FS_OK) {
                return rc;
            }
            sb->free_blocks--;
            return DATA_BLOCKS_START + i;
        }
    }
    return FS_ERR_NO_SPACE;
}

static int fs_free_data_block(Disk *disk, Superblock *sb, int block_num) {
    if (block_num < DATA_BLOCKS_START || block_num > DATA_BLOCKS_END) {
        return FS_ERR_OUT_OF_RANGE;
    }
    int bit_index = block_num - DATA_BLOCKS_START;
    uint8_t bitmap[BLOCK_SIZE];
    int rc = read_block(disk, DATA_BITMAP_BLOCK, bitmap);
    if (rc != FS_OK) {
        return rc;
    }
    if (bitmap_test(bitmap, bit_index)) {
        bitmap_clear(bitmap, bit_index);
        rc = write_block(disk, DATA_BITMAP_BLOCK, bitmap);
        if (rc != FS_OK) {
            return rc;
        }
        if (sb->free_blocks < TOTAL_DATA_BLOCKS) {
            sb->free_blocks++;
        }
    }
    /* Limpiar contenido del bloque liberado */
    uint8_t zero[BLOCK_SIZE];
    memset(zero, 0, sizeof(zero));
    return write_block(disk, block_num, zero);
}

/* ========================================================================== */
/* Lectura/escritura de datos de un inodo (4 directos + 1 indirecto simple)   */
/* ========================================================================== */

static bool is_valid_data_block(int block_num) {
    return block_num >= DATA_BLOCKS_START && block_num <= DATA_BLOCKS_END;
}

static int fs_inode_load_indirect(const Disk *disk, int indirect_block,
                                  int32_t ptrs[INDIRECT_PTRS_PER_BLOCK]) {
    if (indirect_block == INVALID_BLOCK) {
        for (int i = 0; i < INDIRECT_PTRS_PER_BLOCK; ++i) {
            ptrs[i] = INVALID_BLOCK;
        }
        return FS_OK;
    }
    if (!is_valid_data_block(indirect_block)) {
        return FS_ERR_CORRUPT;
    }
    uint8_t raw[BLOCK_SIZE];
    int rc = read_block(disk, indirect_block, raw);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(ptrs, raw, BLOCK_SIZE);
    return FS_OK;
}

static int fs_inode_save_indirect(Disk *disk, int indirect_block,
                                  const int32_t ptrs[INDIRECT_PTRS_PER_BLOCK]) {
    if (!is_valid_data_block(indirect_block)) {
        return FS_ERR_CORRUPT;
    }
    uint8_t raw[BLOCK_SIZE];
    memcpy(raw, ptrs, BLOCK_SIZE);
    return write_block(disk, indirect_block, raw);
}

static int fs_inode_count_phys_blocks(const Disk *disk, const Inode *inode, uint32_t *out_count) {
    uint32_t count = 0;
    for (int i = 0; i < INODE_DIRECT_PTRS; ++i) {
        if (inode->direct[i] != INVALID_BLOCK) {
            if (!is_valid_data_block(inode->direct[i])) {
                return FS_ERR_CORRUPT;
            }
            count++;
        }
    }
    if (inode->indirect != INVALID_BLOCK) {
        if (!is_valid_data_block(inode->indirect)) {
            return FS_ERR_CORRUPT;
        }
        count++; /* Bloque de punteros indirectos */
        int32_t ptrs[INDIRECT_PTRS_PER_BLOCK];
        int rc = fs_inode_load_indirect(disk, inode->indirect, ptrs);
        if (rc != FS_OK) {
            return rc;
        }
        for (int i = 0; i < INDIRECT_PTRS_PER_BLOCK; ++i) {
            if (ptrs[i] != INVALID_BLOCK) {
                if (!is_valid_data_block(ptrs[i])) {
                    return FS_ERR_CORRUPT;
                }
                count++;
            }
        }
    }
    *out_count = count;
    return FS_OK;
}

static int fs_inode_free_all_blocks(Disk *disk, Superblock *sb, Inode *inode) {
    for (int i = 0; i < INODE_DIRECT_PTRS; ++i) {
        if (inode->direct[i] != INVALID_BLOCK) {
            int rc = fs_free_data_block(disk, sb, inode->direct[i]);
            if (rc != FS_OK) {
                return rc;
            }
            inode->direct[i] = INVALID_BLOCK;
        }
    }
    if (inode->indirect != INVALID_BLOCK) {
        int32_t ptrs[INDIRECT_PTRS_PER_BLOCK];
        int rc = fs_inode_load_indirect(disk, inode->indirect, ptrs);
        if (rc != FS_OK) {
            return rc;
        }
        for (int i = 0; i < INDIRECT_PTRS_PER_BLOCK; ++i) {
            if (ptrs[i] != INVALID_BLOCK) {
                rc = fs_free_data_block(disk, sb, ptrs[i]);
                if (rc != FS_OK) {
                    return rc;
                }
            }
        }
        rc = fs_free_data_block(disk, sb, inode->indirect);
        if (rc != FS_OK) {
            return rc;
        }
        inode->indirect = INVALID_BLOCK;
    }
    inode->size = 0;
    inode->block_count = 0;
    return FS_OK;
}

static int fs_inode_write_data(Disk *disk, Superblock *sb, Inode *inode,
                               const uint8_t *data, uint32_t size) {
    if (!disk || !sb || !inode || (size > 0 && !data)) {
        return FS_ERR_INVALID_ARG;
    }

    uint32_t max_logical = (uint32_t)(INODE_DIRECT_PTRS + INDIRECT_PTRS_PER_BLOCK);
    uint32_t needed_data = (size == 0) ? 0u : ((size + BLOCK_SIZE - 1u) / BLOCK_SIZE);
    if (needed_data > max_logical) {
        return FS_ERR_TOO_LARGE;
    }
    bool need_indirect = (needed_data > (uint32_t)INODE_DIRECT_PTRS);
    uint32_t needed_phys = needed_data + (need_indirect ? 1u : 0u);

    uint32_t old_phys = 0;
    int rc = fs_inode_count_phys_blocks(disk, inode, &old_phys);
    if (rc != FS_OK) {
        return rc;
    }

    /* Verificación previa para garantizar que no haya cambios parciales sin espacio */
    if (needed_phys > old_phys) {
        uint32_t extra = needed_phys - old_phys;
        if (extra > sb->free_blocks) {
            return FS_ERR_NO_SPACE;
        }
    }

    int32_t indirect_ptrs[INDIRECT_PTRS_PER_BLOCK];
    rc = fs_inode_load_indirect(disk, inode->indirect, indirect_ptrs);
    if (rc != FS_OK) {
        return rc;
    }

    /* Fase 1: liberar bloques sobrantes si el nuevo tamaño es menor */
    if (inode->indirect != INVALID_BLOCK) {
        uint32_t keep_indirect = need_indirect ? (needed_data - (uint32_t)INODE_DIRECT_PTRS) : 0u;
        for (uint32_t i = keep_indirect; i < (uint32_t)INDIRECT_PTRS_PER_BLOCK; ++i) {
            if (indirect_ptrs[i] != INVALID_BLOCK) {
                rc = fs_free_data_block(disk, sb, indirect_ptrs[i]);
                if (rc != FS_OK) {
                    return rc;
                }
                indirect_ptrs[i] = INVALID_BLOCK;
            }
        }
        if (!need_indirect) {
            rc = fs_free_data_block(disk, sb, inode->indirect);
            if (rc != FS_OK) {
                return rc;
            }
            inode->indirect = INVALID_BLOCK;
        }
    }
    for (uint32_t i = needed_data; i < (uint32_t)INODE_DIRECT_PTRS; ++i) {
        if (inode->direct[i] != INVALID_BLOCK) {
            rc = fs_free_data_block(disk, sb, inode->direct[i]);
            if (rc != FS_OK) {
                return rc;
            }
            inode->direct[i] = INVALID_BLOCK;
        }
    }

    /* Fase 2: asignar bloques directos faltantes */
    uint32_t direct_needed = (needed_data < (uint32_t)INODE_DIRECT_PTRS)
                                 ? needed_data
                                 : (uint32_t)INODE_DIRECT_PTRS;
    for (uint32_t i = 0; i < direct_needed; ++i) {
        if (inode->direct[i] == INVALID_BLOCK) {
            int blk = fs_alloc_data_block(disk, sb);
            if (blk < 0) {
                return blk;
            }
            inode->direct[i] = blk;
        }
    }

    /* Fase 3: asignar bloque indirecto y punteros indirectos si se requieren */
    if (need_indirect) {
        if (inode->indirect == INVALID_BLOCK) {
            int iblk = fs_alloc_data_block(disk, sb);
            if (iblk < 0) {
                return iblk;
            }
            inode->indirect = iblk;
        }
        uint32_t indirect_needed = needed_data - (uint32_t)INODE_DIRECT_PTRS;
        for (uint32_t i = 0; i < indirect_needed; ++i) {
            if (indirect_ptrs[i] == INVALID_BLOCK) {
                int blk = fs_alloc_data_block(disk, sb);
                if (blk < 0) {
                    return blk;
                }
                indirect_ptrs[i] = blk;
            }
        }
        rc = fs_inode_save_indirect(disk, inode->indirect, indirect_ptrs);
        if (rc != FS_OK) {
            return rc;
        }
    }

    /* Fase 4: escribir el contenido bloque por bloque (con relleno cero al final) */
    for (uint32_t blk = 0; blk < needed_data; ++blk) {
        int phys = (blk < (uint32_t)INODE_DIRECT_PTRS)
                       ? inode->direct[blk]
                       : indirect_ptrs[blk - (uint32_t)INODE_DIRECT_PTRS];
        uint8_t buf[BLOCK_SIZE];
        memset(buf, 0, sizeof(buf));
        uint32_t offset = blk * BLOCK_SIZE;
        uint32_t chunk = size - offset;
        if (chunk > BLOCK_SIZE) {
            chunk = BLOCK_SIZE;
        }
        memcpy(buf, data + offset, chunk);
        rc = write_block(disk, phys, buf);
        if (rc != FS_OK) {
            return rc;
        }
    }

    inode->size = size;
    inode->block_count = needed_phys;
    return FS_OK;
}

static int fs_inode_read_data(const Disk *disk, const Inode *inode,
                              uint8_t *out_buf, uint32_t max_len, uint32_t *out_size) {
    if (!disk || !inode || !out_size || (inode->size > 0 && !out_buf)) {
        return FS_ERR_INVALID_ARG;
    }
    if (inode->size > max_len) {
        return FS_ERR_BUFFER_TOO_SMALL;
    }
    if (inode->size == 0) {
        *out_size = 0;
        return FS_OK;
    }

    uint32_t num_blocks = (inode->size + BLOCK_SIZE - 1u) / BLOCK_SIZE;
    int32_t indirect_ptrs[INDIRECT_PTRS_PER_BLOCK];
    if (num_blocks > (uint32_t)INODE_DIRECT_PTRS) {
        if (inode->indirect == INVALID_BLOCK) {
            return FS_ERR_CORRUPT;
        }
        int rc = fs_inode_load_indirect(disk, inode->indirect, indirect_ptrs);
        if (rc != FS_OK) {
            return rc;
        }
    }

    for (uint32_t blk = 0; blk < num_blocks; ++blk) {
        int phys = (blk < (uint32_t)INODE_DIRECT_PTRS)
                       ? inode->direct[blk]
                       : indirect_ptrs[blk - (uint32_t)INODE_DIRECT_PTRS];
        if (!is_valid_data_block(phys)) {
            return FS_ERR_CORRUPT;
        }
        uint8_t buf[BLOCK_SIZE];
        int rc = read_block(disk, phys, buf);
        if (rc != FS_OK) {
            return rc;
        }
        uint32_t offset = blk * BLOCK_SIZE;
        uint32_t chunk = inode->size - offset;
        if (chunk > BLOCK_SIZE) {
            chunk = BLOCK_SIZE;
        }
        memcpy(out_buf + offset, buf, chunk);
    }

    *out_size = inode->size;
    return FS_OK;
}

/* ========================================================================== */
/* Directorio raíz (Bloque 11) y operaciones de alto nivel sobre ROAs         */
/* ========================================================================== */

static int fs_read_root_dir(const Disk *disk, DirEntry entries[ROOT_DIR_MAX_ENTRIES]) {
    uint8_t raw[BLOCK_SIZE];
    int rc = read_block(disk, ROOT_DIR_BLOCK, raw);
    if (rc != FS_OK) {
        return rc;
    }
    memcpy(entries, raw, BLOCK_SIZE);
    return FS_OK;
}

static int fs_write_root_dir(Disk *disk, const DirEntry entries[ROOT_DIR_MAX_ENTRIES]) {
    uint8_t raw[BLOCK_SIZE];
    memcpy(raw, entries, BLOCK_SIZE);
    return write_block(disk, ROOT_DIR_BLOCK, raw);
}

static int fs_dir_find_slot(const DirEntry entries[ROOT_DIR_MAX_ENTRIES], const char *ip) {
    for (int i = 0; i < ROOT_DIR_MAX_ENTRIES; ++i) {
        if (entries[i].in_use && strcmp(entries[i].ip, ip) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Guardar o reemplazar un ROA (hasta 512 B) asociado a una IP.
 * Incrementa el contador de versión en el superbloque.
 */
static int fs_roa_put(Disk *disk, const char *ip, const char *owner,
                      const uint8_t *roa_data, uint32_t roa_size,
                      bool *out_replaced) {
    if (!disk || !is_valid_ip_key(ip) || (roa_size > 0 && !roa_data)) {
        return FS_ERR_INVALID_ARG;
    }
    if (roa_size > MAX_ROA_SIZE) {
        return FS_ERR_TOO_LARGE;
    }

    Superblock sb;
    int rc = fs_read_superblock(disk, &sb);
    if (rc != FS_OK) {
        return rc;
    }

    DirEntry dir[ROOT_DIR_MAX_ENTRIES];
    rc = fs_read_root_dir(disk, dir);
    if (rc != FS_OK) {
        return rc;
    }

    int64_t now = (int64_t)time(NULL);
    int existing_slot = fs_dir_find_slot(dir, ip);

    if (existing_slot >= 0) {
        /* Reemplazar ROA existente */
        int inode_idx = dir[existing_slot].inode_index;
        Inode inode;
        rc = fs_read_inode(disk, inode_idx, &inode);
        if (rc != FS_OK) {
            return rc;
        }
        if (!inode.in_use) {
            return FS_ERR_CORRUPT;
        }

        rc = fs_inode_write_data(disk, &sb, &inode, roa_data, roa_size);
        if (rc != FS_OK) {
            return rc;
        }

        inode.modified_at = now;
        inode.accessed_at = now;
        if (owner && owner[0] != '\0') {
            safe_copy_str(inode.owner, sizeof(inode.owner), owner);
        }

        rc = fs_write_inode(disk, inode_idx, &inode);
        if (rc != FS_OK) {
            return rc;
        }

        sb.version++;
        rc = fs_write_superblock(disk, &sb);
        if (rc != FS_OK) {
            return rc;
        }

        if (out_replaced) {
            *out_replaced = true;
        }
        return FS_OK;
    }

    /* Crear nuevo ROA */
    int free_slot = -1;
    for (int i = 0; i < ROOT_DIR_MAX_ENTRIES; ++i) {
        if (!dir[i].in_use) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        return FS_ERR_DIR_FULL;
    }

    uint32_t needed_blocks = (roa_size == 0) ? 0u : ((roa_size + BLOCK_SIZE - 1u) / BLOCK_SIZE);
    if (sb.free_inodes == 0) {
        return FS_ERR_NO_INODES;
    }
    if (needed_blocks > sb.free_blocks) {
        return FS_ERR_NO_SPACE;
    }

    int inode_idx = fs_alloc_inode(disk, &sb);
    if (inode_idx < 0) {
        return inode_idx;
    }

    Inode inode;
    inode_reset_empty(&inode);
    inode.in_use      = 1;
    inode.type        = 1;
    inode.permissions = 0644;
    inode.created_at  = now;
    inode.modified_at = now;
    inode.accessed_at = now;
    safe_copy_str(inode.owner, sizeof(inode.owner),
                  (owner && owner[0] != '\0') ? owner : "admin");

    rc = fs_inode_write_data(disk, &sb, &inode, roa_data, roa_size);
    if (rc != FS_OK) {
        (void)fs_free_inode(disk, &sb, inode_idx);
        return rc;
    }

    rc = fs_write_inode(disk, inode_idx, &inode);
    if (rc != FS_OK) {
        return rc;
    }

    memset(&dir[free_slot], 0, sizeof(DirEntry));
    safe_copy_str(dir[free_slot].ip, sizeof(dir[free_slot].ip), ip);
    dir[free_slot].inode_index = inode_idx;
    dir[free_slot].in_use      = 1;

    rc = fs_write_root_dir(disk, dir);
    if (rc != FS_OK) {
        return rc;
    }

    sb.version++;
    rc = fs_write_superblock(disk, &sb);
    if (rc != FS_OK) {
        return rc;
    }

    if (out_replaced) {
        *out_replaced = false;
    }
    return FS_OK;
}

/* Consultar un ROA por IP */
static int fs_roa_get(Disk *disk, const char *ip,
                      uint8_t *out_data, uint32_t max_len, uint32_t *out_size,
                      Inode *out_inode, int *out_inode_idx) {
    if (!disk || !is_valid_ip_key(ip) || !out_data || !out_size) {
        return FS_ERR_INVALID_ARG;
    }

    Superblock sb;
    int rc = fs_read_superblock(disk, &sb);
    if (rc != FS_OK) {
        return rc;
    }

    DirEntry dir[ROOT_DIR_MAX_ENTRIES];
    rc = fs_read_root_dir(disk, dir);
    if (rc != FS_OK) {
        return rc;
    }

    int slot = fs_dir_find_slot(dir, ip);
    if (slot < 0) {
        return FS_ERR_NOT_FOUND;
    }

    int inode_idx = dir[slot].inode_index;
    Inode inode;
    rc = fs_read_inode(disk, inode_idx, &inode);
    if (rc != FS_OK) {
        return rc;
    }
    if (!inode.in_use) {
        return FS_ERR_CORRUPT;
    }

    rc = fs_inode_read_data(disk, &inode, out_data, max_len, out_size);
    if (rc != FS_OK) {
        return rc;
    }

    /* Actualizar timestamp de acceso */
    inode.accessed_at = (int64_t)time(NULL);
    rc = fs_write_inode(disk, inode_idx, &inode);
    if (rc != FS_OK) {
        return rc;
    }

    if (out_inode) {
        *out_inode = inode;
    }
    if (out_inode_idx) {
        *out_inode_idx = inode_idx;
    }
    return FS_OK;
}

/* Borrar un ROA por IP e incrementar contador de versión */
static int fs_roa_delete(Disk *disk, const char *ip) {
    if (!disk || !is_valid_ip_key(ip)) {
        return FS_ERR_INVALID_ARG;
    }

    Superblock sb;
    int rc = fs_read_superblock(disk, &sb);
    if (rc != FS_OK) {
        return rc;
    }

    DirEntry dir[ROOT_DIR_MAX_ENTRIES];
    rc = fs_read_root_dir(disk, dir);
    if (rc != FS_OK) {
        return rc;
    }

    int slot = fs_dir_find_slot(dir, ip);
    if (slot < 0) {
        return FS_ERR_NOT_FOUND;
    }

    int inode_idx = dir[slot].inode_index;
    Inode inode;
    rc = fs_read_inode(disk, inode_idx, &inode);
    if (rc != FS_OK) {
        return rc;
    }

    /*
     * CORREGIDO (error 2): la versión de la IA liberaba bloque por bloque y, si
     * encontraba un puntero inválido a la mitad, salía con error dejando los
     * bitmaps ya modificados, el superbloque sin actualizar y el directorio
     * apuntando a un inodo con bloques liberados. Ahora se valida todo el inodo
     * ANTES de modificar nada (y, como en get/put, se exige in_use).
     */
    if (!inode.in_use) {
        return FS_ERR_CORRUPT;
    }
    uint32_t phys_blocks = 0;
    rc = fs_inode_count_phys_blocks(disk, &inode, &phys_blocks);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fs_inode_free_all_blocks(disk, &sb, &inode);
    if (rc != FS_OK) {
        return rc;
    }

    inode_reset_empty(&inode);
    rc = fs_write_inode(disk, inode_idx, &inode);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fs_free_inode(disk, &sb, inode_idx);
    if (rc != FS_OK) {
        return rc;
    }

    memset(&dir[slot], 0, sizeof(DirEntry));
    dir[slot].inode_index = INVALID_INODE;
    dir[slot].in_use = 0;

    rc = fs_write_root_dir(disk, dir);
    if (rc != FS_OK) {
        return rc;
    }

    sb.version++;
    return fs_write_superblock(disk, &sb);
}

/* Listar todos los ROAs almacenados en el directorio raíz */
static int fs_roa_list(const Disk *disk, FILE *out) {
    if (!disk || !out) {
        return FS_ERR_INVALID_ARG;
    }

    Superblock sb;
    int rc = fs_read_superblock(disk, &sb);
    if (rc != FS_OK) {
        return rc;
    }

    DirEntry dir[ROOT_DIR_MAX_ENTRIES];
    rc = fs_read_root_dir(disk, dir);
    if (rc != FS_OK) {
        return rc;
    }

    fprintf(out, "\n--- Listado de ROAs (versión del FS: %" PRIu32 ") ---\n", sb.version);
    fprintf(out, "%-4s %-22s %-6s %-15s %-6s %-8s %-19s %s\n",
            "Slot", "IP", "Inodo", "Dueño", "Bytes", "Bloque", "Modificado", "Vista previa");
    fprintf(out, "---------------------------------------------------------------------------------------------------------\n");

    int count = 0;
    for (int i = 0; i < ROOT_DIR_MAX_ENTRIES; ++i) {
        if (!dir[i].in_use) {
            continue;
        }
        Inode inode;
        rc = fs_read_inode(disk, dir[i].inode_index, &inode);
        if (rc != FS_OK) {
            return rc;
        }

        char ts_buf[32];
        format_timestamp(inode.modified_at, ts_buf, sizeof(ts_buf));

        char preview[32];
        uint8_t roa_buf[MAX_ROA_SIZE];
        uint32_t roa_size = 0;
        if (inode.size > 0 && inode.size <= MAX_ROA_SIZE &&
            fs_inode_read_data(disk, &inode, roa_buf, sizeof(roa_buf), &roa_size) == FS_OK) {
            uint32_t show = (roa_size < 24u) ? roa_size : 24u;
            for (uint32_t j = 0; j < show; ++j) {
                preview[j] = isprint(roa_buf[j]) ? (char)roa_buf[j] : '.';
            }
            if (roa_size > 24u) {
                preview[show]     = '.';
                preview[show + 1] = '.';
                preview[show + 2] = '.';
                preview[show + 3] = '\0';
            } else {
                preview[show] = '\0';
            }
        } else if (inode.size == 0) {
            safe_copy_str(preview, sizeof(preview), "(vacío)");
        } else {
            safe_copy_str(preview, sizeof(preview), "(error lectura)");
        }

        char blk_buf[16];
        if (inode.direct[0] != INVALID_BLOCK) {
            snprintf(blk_buf, sizeof(blk_buf), "%" PRId32, inode.direct[0]);
        } else {
            safe_copy_str(blk_buf, sizeof(blk_buf), "-");
        }

        fprintf(out, "%-4d %-22s %-6" PRId32 " %-14.14s %-6" PRIu32 " %-8s %-19s %s\n",
                i, dir[i].ip, dir[i].inode_index, inode.owner,
                inode.size, blk_buf, ts_buf, preview);
        count++;
    }

    if (count == 0) {
        fprintf(out, "(No hay ROAs almacenados)\n");
    }
    fprintf(out, "Total de ROAs: %d / %d\n", count, ROOT_DIR_MAX_ENTRIES);
    return count;
}

/* ========================================================================== */
/* Visualización de estado del disco y volcados hexadecimales                 */
/* ========================================================================== */

static const char *block_region_name(int block_num) {
    if (block_num == SUPERBLOCK_BLOCK)   return "Superbloque";
    if (block_num == INODE_BITMAP_BLOCK) return "Bitmap de inodos";
    if (block_num == DATA_BITMAP_BLOCK)  return "Bitmap de bloques de datos";
    if (block_num >= INODE_TABLE_START &&
        block_num < INODE_TABLE_START + INODE_TABLE_BLOCKS) {
        return "Tabla de inodos";
    }
    if (block_num == ROOT_DIR_BLOCK)     return "Directorio raíz";
    if (block_num >= DATA_BLOCKS_START && block_num <= DATA_BLOCKS_END) {
        return "Bloque de datos";
    }
    return "Fuera de rango";
}

static void print_hexdump(const uint8_t *data, size_t len) {
    for (size_t offset = 0; offset < len; offset += 16) {
        printf("  %04zX | ", offset);
        for (size_t i = 0; i < 16; ++i) {
            if (offset + i < len) {
                printf("%02X ", data[offset + i]);
            } else {
                printf("   ");
            }
            if (i == 7) {
                printf(" ");
            }
        }
        printf("| ");
        for (size_t i = 0; i < 16 && offset + i < len; ++i) {
            uint8_t ch = data[offset + i];
            putchar(isprint(ch) ? (int)ch : '.');
        }
        putchar('\n');
    }
}

static void fs_print_status(const Disk *disk) {
    printf("\n==================== ESTADO DEL DISCO SIMULADO ====================\n");
    printf("Geometría física : %d platos x %d pistas x %d sectores x %d B = %d B (32 KB)\n",
           DISK_PLATTERS, DISK_TRACKS_PER_PLATTER, DISK_SECTORS_PER_TRACK,
           SECTOR_SIZE, DISK_SIZE_BYTES);
    printf("Capa lógica      : %d bloques de %d B (%d sectores/bloque)\n",
           TOTAL_BLOCKS, BLOCK_SIZE, SECTORS_PER_BLOCK);

    Superblock sb;
    uint8_t raw_sb[BLOCK_SIZE];
    (void)read_block(disk, SUPERBLOCK_BLOCK, raw_sb);
    memcpy(&sb, raw_sb, sizeof(Superblock));

    bool valid = (fs_read_superblock(disk, &sb) == FS_OK);
    printf("\n[Bloque 0: Superbloque]\n");
    printf("  Número mágico   : 0x%08" PRIX32 " (%s)\n",
           sb.magic, valid ? "VÁLIDO" : "INVÁLIDO / NO FORMATEADO");
    if (!valid) {
        printf("  El sistema de archivos no está inicializado. Use la opción Formatear disco.\n");
        printf("===================================================================\n");
        return;
    }

    printf("  Versión         : %" PRIu32 "\n", sb.version);
    printf("  Bloques totales : %" PRIu32 " (12 metadatos + %" PRIu32 " datos)\n",
           sb.total_blocks, sb.data_blocks);
    printf("  Bloques libres  : %" PRIu32 " / %" PRIu32 " bloques de datos\n",
           sb.free_blocks, sb.data_blocks);
    printf("  Inodos totales  : %" PRIu32 "\n", sb.total_inodes);
    printf("  Inodos libres   : %" PRIu32 " / %" PRIu32 "\n",
           sb.free_inodes, sb.total_inodes);

    /* Bitmap de inodos */
    uint8_t inode_bm[BLOCK_SIZE];
    (void)read_block(disk, INODE_BITMAP_BLOCK, inode_bm);
    printf("\n[Bloque 1: Bitmap de inodos (0..31)] (1=ocupado, 0=libre)\n  ");
    for (int i = 0; i < TOTAL_INODES; ++i) {
        putchar(bitmap_test(inode_bm, i) ? '1' : '0');
        if ((i + 1) % 8 == 0 && i + 1 < TOTAL_INODES) {
            putchar(' ');
        }
    }
    putchar('\n');

    /* Bitmap de bloques de datos */
    uint8_t data_bm[BLOCK_SIZE];
    (void)read_block(disk, DATA_BITMAP_BLOCK, data_bm);
    printf("\n[Bloque 2: Bitmap de bloques de datos (bloques 12..63)] (1=ocupado, 0=libre)\n");
    for (int row = 0; row < TOTAL_DATA_BLOCKS; row += 16) {
        int end = row + 15;
        if (end >= TOTAL_DATA_BLOCKS) {
            end = TOTAL_DATA_BLOCKS - 1;
        }
        printf("  [%2d..%2d] ", DATA_BLOCKS_START + row, DATA_BLOCKS_START + end);
        for (int i = row; i <= end; ++i) {
            putchar(bitmap_test(data_bm, i) ? '1' : '0');
            if ((i - row) == 7 && i < end) {
                putchar(' ');
            }
        }
        putchar('\n');
    }

    /* Inodos activos */
    printf("\n[Bloques 3-10: Inodos ocupados]\n");
    int active_inodes = 0;
    for (int i = 0; i < TOTAL_INODES; ++i) {
        if (!bitmap_test(inode_bm, i)) {
            continue;
        }
        Inode ino;
        if (fs_read_inode(disk, i, &ino) == FS_OK && ino.in_use) {
            char c_ts[32], m_ts[32];
            format_timestamp(ino.created_at, c_ts, sizeof(c_ts));
            format_timestamp(ino.modified_at, m_ts, sizeof(m_ts));
            printf("  Inodo %2d | dueño=%-12s | tam=%3" PRIu32 " B | dir=[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "] ind=%" PRId32 " | mod=%s\n",
                   i, ino.owner, ino.size,
                   ino.direct[0], ino.direct[1], ino.direct[2], ino.direct[3],
                   ino.indirect, m_ts);
            active_inodes++;
        }
    }
    if (active_inodes == 0) {
        printf("  (Ninguno)\n");
    }

    /* Directorio raíz */
    DirEntry dir[ROOT_DIR_MAX_ENTRIES];
    (void)fs_read_root_dir(disk, dir);
    printf("\n[Bloque 11: Directorio raíz (entradas de 32 B)]\n");
    int active_entries = 0;
    for (int i = 0; i < ROOT_DIR_MAX_ENTRIES; ++i) {
        if (dir[i].in_use) {
            printf("  Entrada %2d: %-23s -> inodo %" PRId32 "\n",
                   i, dir[i].ip, dir[i].inode_index);
            active_entries++;
        }
    }
    if (active_entries == 0) {
        printf("  (Vacío)\n");
    }
    printf("  Entradas usadas: %d / %d\n", active_entries, ROOT_DIR_MAX_ENTRIES);
    printf("===================================================================\n");
}

/* ========================================================================== */
/* Suite de pruebas automáticas (ejecutada en un disco temporal)              */
/* ========================================================================== */

static int run_self_tests(void) {
    Disk tmp;
    int passed = 0;
    int failed = 0;

    printf("\n=========== EJECUTANDO PRUEBAS AUTOMÁTICAS ===========\n");

    /* 1. Detección de disco sin formatear */
    disk_init(&tmp);
    if (!fs_is_formatted(&tmp) &&
        fs_roa_delete(&tmp, "10.0.0.1") == FS_ERR_NOT_FORMATTED) {
        printf("[PASS] 1. Detección de disco no formateado (magic inválido)\n");
        passed++;
    } else {
        printf("[FAIL] 1. Detección de disco no formateado\n");
        failed++;
    }

    /* 2. Biyección CHS <-> LBA en los 256 sectores y validación de rango */
    bool chs_ok = true;
    int seen[TOTAL_SECTORS];
    memset(seen, 0, sizeof(seen));
    for (int c = 0; c < DISK_TRACKS_PER_PLATTER; ++c) {
        for (int h = 0; h < DISK_PLATTERS; ++h) {
            for (int s = 0; s < DISK_SECTORS_PER_TRACK; ++s) {
                int lba = chs_to_lba(c, h, s);
                int c2 = -1, h2 = -1, s2 = -1;
                if (lba < 0 || lba >= TOTAL_SECTORS || seen[lba] != 0 ||
                    lba_to_chs(lba, &c2, &h2, &s2) != FS_OK ||
                    c2 != c || h2 != h || s2 != s) {
                    chs_ok = false;
                } else {
                    seen[lba] = 1;
                }
            }
        }
    }
    if (chs_to_lba(-1, 0, 0) >= 0 || chs_to_lba(16, 0, 0) >= 0 ||
        chs_to_lba(0, 2, 0) >= 0 || chs_to_lba(0, 0, 8) >= 0) {
        chs_ok = false;
    }
    if (chs_ok) {
        printf("[PASS] 2. Traducción CHS <-> LBA (256 sectores + fuera de rango)\n");
        passed++;
    } else {
        printf("[FAIL] 2. Traducción CHS <-> LBA\n");
        failed++;
    }

    /* 3. Lectura/escritura de sector (CHS y LBA) y bloque */
    uint8_t sec_w[SECTOR_SIZE], sec_r[SECTOR_SIZE];
    for (int i = 0; i < SECTOR_SIZE; ++i) {
        sec_w[i] = (uint8_t)(i ^ 0xA5);
    }
    int test_lba = chs_to_lba(5, 1, 3);
    bool io_ok = (write_sector_chs(&tmp, 5, 1, 3, sec_w) == FS_OK &&
                  read_sector_lba(&tmp, test_lba, sec_r) == FS_OK &&
                  memcmp(sec_w, sec_r, SECTOR_SIZE) == 0);
    uint8_t blk_w[BLOCK_SIZE], blk_r[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        blk_w[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    }
    io_ok = io_ok &&
            (write_block(&tmp, 20, blk_w) == FS_OK &&
             read_block(&tmp, 20, blk_r) == FS_OK &&
             memcmp(blk_w, blk_r, BLOCK_SIZE) == 0);
    if (io_ok) {
        printf("[PASS] 3. Lectura/escritura de sector (CHS/LBA) y bloque (512 B)\n");
        passed++;
    } else {
        printf("[FAIL] 3. Lectura/escritura de sector y bloque\n");
        failed++;
    }

    /* 4. Formateo y verificación del superbloque */
    Superblock sb;
    if (fs_format(&tmp) == FS_OK &&
        fs_read_superblock(&tmp, &sb) == FS_OK &&
        sb.magic == FS_MAGIC && sb.version == 0 &&
        sb.total_blocks == 64 && sb.free_blocks == 52 &&
        sb.total_inodes == 32 && sb.free_inodes == 32) {
        printf("[PASS] 4. Formateo del disco y verificación de superbloque\n");
        passed++;
    } else {
        printf("[FAIL] 4. Formateo del disco\n");
        failed++;
    }

    /* 5. Put, Get, Replace (incluyendo ROA de 512 B) y versión */
    uint8_t payload512[MAX_ROA_SIZE];
    for (int i = 0; i < MAX_ROA_SIZE; ++i) {
        payload512[i] = (uint8_t)('A' + (i % 26));
    }
    uint8_t read_buf[MAX_ROA_SIZE];
    uint32_t read_sz = 0;
    bool replaced = true;
    Inode ino;
    int ino_idx = -1;

    const char *roa_v1 = "ASN=64512;PREFIX=192.168.1.0/24;MAXLEN=24";
    bool roa_ok = (fs_roa_put(&tmp, "192.168.1.0/24", "AS64512",
                              (const uint8_t *)roa_v1, (uint32_t)strlen(roa_v1),
                              &replaced) == FS_OK && !replaced);
    roa_ok = roa_ok &&
             (fs_read_superblock(&tmp, &sb) == FS_OK && sb.version == 1 &&
              sb.free_blocks == 51 && sb.free_inodes == 31);
    roa_ok = roa_ok &&
             (fs_roa_get(&tmp, "192.168.1.0/24", read_buf, sizeof(read_buf),
                         &read_sz, &ino, &ino_idx) == FS_OK &&
              read_sz == strlen(roa_v1) &&
              memcmp(read_buf, roa_v1, read_sz) == 0 &&
              strcmp(ino.owner, "AS64512") == 0);

    /* Reemplazar por ROA exacto de 512 B */
    roa_ok = roa_ok &&
             (fs_roa_put(&tmp, "192.168.1.0/24", "AS64512-NEW",
                         payload512, MAX_ROA_SIZE, &replaced) == FS_OK && replaced);
    roa_ok = roa_ok &&
             (fs_read_superblock(&tmp, &sb) == FS_OK && sb.version == 2 &&
              sb.free_blocks == 51 && sb.free_inodes == 31);
    roa_ok = roa_ok &&
             (fs_roa_get(&tmp, "192.168.1.0/24", read_buf, sizeof(read_buf),
                         &read_sz, &ino, &ino_idx) == FS_OK &&
              read_sz == MAX_ROA_SIZE &&
              memcmp(read_buf, payload512, MAX_ROA_SIZE) == 0 &&
              strcmp(ino.owner, "AS64512-NEW") == 0);

    /* Rechazar ROA > 512 B */
    uint8_t too_big[MAX_ROA_SIZE + 1];
    memset(too_big, 'X', sizeof(too_big));
    roa_ok = roa_ok &&
             (fs_roa_put(&tmp, "10.0.0.0/8", "root", too_big, sizeof(too_big), NULL) == FS_ERR_TOO_LARGE);

    if (roa_ok) {
        printf("[PASS] 5. Operaciones put / get / replace (hasta 512 B) y control de versión\n");
        passed++;
    } else {
        printf("[FAIL] 5. Operaciones put / get / replace\n");
        failed++;
    }

    /* 6. Delete y liberación de recursos */
    bool del_ok = (fs_roa_delete(&tmp, "192.168.1.0/24") == FS_OK &&
                   fs_read_superblock(&tmp, &sb) == FS_OK &&
                   sb.version == 3 && sb.free_blocks == 52 && sb.free_inodes == 32 &&
                   fs_roa_get(&tmp, "192.168.1.0/24", read_buf, sizeof(read_buf),
                              &read_sz, NULL, NULL) == FS_ERR_NOT_FOUND);
    if (del_ok) {
        printf("[PASS] 6. Operación delete, liberación de inodo/bloque y versión\n");
        passed++;
    } else {
        printf("[FAIL] 6. Operación delete\n");
        failed++;
    }

    /* 7. Llenado del directorio raíz (16 entradas) y límite */
    bool cap_ok = true;
    for (int i = 0; i < ROOT_DIR_MAX_ENTRIES; ++i) {
        char ip_buf[24];
        char val_buf[64];
        snprintf(ip_buf, sizeof(ip_buf), "10.0.%d.0/24", i);
        snprintf(val_buf, sizeof(val_buf), "ASN=%d;PREFIX=%s", 65000 + i, ip_buf);
        if (fs_roa_put(&tmp, ip_buf, "RPKI", (const uint8_t *)val_buf,
                       (uint32_t)strlen(val_buf), &replaced) != FS_OK || replaced) {
            cap_ok = false;
        }
    }
    /* La entrada 17 debe fallar con FS_ERR_DIR_FULL */
    if (fs_roa_put(&tmp, "10.0.99.0/24", "RPKI", (const uint8_t *)"X", 1, NULL) != FS_ERR_DIR_FULL) {
        cap_ok = false;
    }
    /* Reemplazar una existente con directorio lleno debe funcionar */
    if (fs_roa_put(&tmp, "10.0.5.0/24", "RPKI", (const uint8_t *)"UPDATED", 7, &replaced) != FS_OK || !replaced) {
        cap_ok = false;
    }
    if (cap_ok) {
        printf("[PASS] 7. Capacidad del directorio raíz (16 entradas + reemplazo lleno)\n");
        passed++;
    } else {
        printf("[FAIL] 7. Capacidad del directorio raíz\n");
        failed++;
    }

    /* 8. Punteros directos (4) + puntero indirecto simple (1) en inodo */
    (void)fs_format(&tmp);
    (void)fs_read_superblock(&tmp, &sb);
    int ind_ino_idx = fs_alloc_inode(&tmp, &sb);
    Inode big_ino;
    inode_reset_empty(&big_ino);
    big_ino.in_use = 1;
    uint8_t big_w[3000], big_r[3000];
    for (size_t i = 0; i < sizeof(big_w); ++i) {
        big_w[i] = (uint8_t)((i * 31 + 7) & 0xFF);
    }
    uint32_t big_sz = 0;
    /* 3000 B requiere 6 bloques de datos (4 directos + 2 indirectos) + 1 bloque tabla indirecta = 7 bloques */
    bool ind_ok = (ind_ino_idx >= 0 &&
                   fs_inode_write_data(&tmp, &sb, &big_ino, big_w, (uint32_t)sizeof(big_w)) == FS_OK &&
                   big_ino.indirect != INVALID_BLOCK &&
                   big_ino.block_count == 7 &&
                   sb.free_blocks == TOTAL_DATA_BLOCKS - 7 &&
                   fs_inode_read_data(&tmp, &big_ino, big_r, (uint32_t)sizeof(big_r), &big_sz) == FS_OK &&
                   big_sz == sizeof(big_w) &&
                   memcmp(big_w, big_r, sizeof(big_w)) == 0 &&
                   fs_inode_free_all_blocks(&tmp, &sb, &big_ino) == FS_OK &&
                   sb.free_blocks == TOTAL_DATA_BLOCKS);
    if (ind_ok) {
        printf("[PASS] 8. Punteros directos (4) + puntero indirecto simple (3000 B -> 7 bloques)\n");
        passed++;
    } else {
        printf("[FAIL] 8. Punteros directos e indirecto simple\n");
        failed++;
    }

    /* 9. (NUEVA) Validación estricta de claves IPv4 */
    (void)fs_format(&tmp);
    const char *good_ips[] = {"192.168.1.0/24", "10.0.0.1", "0.0.0.0/0", "255.255.255.255/32"};
    const char *bad_ips[]  = {"hola", "999.999.999.999/99", "1.2.3.4/xx", "256.1.1.1",
                              "10.0.0.0/33", "1.2.3", "1.2.3.4/", "///", "1.2.3.4 /24"};
    bool ip_ok = true;
    for (size_t i = 0; i < sizeof(good_ips) / sizeof(good_ips[0]); ++i) {
        if (fs_roa_put(&tmp, good_ips[i], "t", (const uint8_t *)"A", 1, NULL) != FS_OK) {
            ip_ok = false;
        }
    }
    for (size_t i = 0; i < sizeof(bad_ips) / sizeof(bad_ips[0]); ++i) {
        if (fs_roa_put(&tmp, bad_ips[i], "t", (const uint8_t *)"A", 1, NULL) != FS_ERR_INVALID_ARG) {
            ip_ok = false;
        }
    }
    if (ip_ok) {
        printf("[PASS] 9. Validación estricta de IP/prefijo IPv4\n");
        passed++;
    } else {
        printf("[FAIL] 9. Validación estricta de IP/prefijo IPv4\n");
        failed++;
    }

    /* 10. (NUEVA) Un delete que falla por inodo corrupto no debe dejar el FS a medias */
    (void)fs_format(&tmp);
    bool atom_ok = (fs_roa_put(&tmp, "10.0.0.0/8", "o", (const uint8_t *)"A", 1, NULL) == FS_OK);
    Superblock sb10;
    Inode ino10;
    atom_ok = atom_ok && fs_read_superblock(&tmp, &sb10) == FS_OK &&
              fs_read_inode(&tmp, 0, &ino10) == FS_OK;
    int extra_blk = atom_ok ? fs_alloc_data_block(&tmp, &sb10) : -1;
    if (extra_blk >= 0) {
        (void)fs_write_superblock(&tmp, &sb10);
        ino10.direct[1] = extra_blk;
        ino10.direct[2] = 5; /* puntero corrupto: bloque de metadatos */
        (void)fs_write_inode(&tmp, 0, &ino10);
        uint8_t dbm10[BLOCK_SIZE];
        atom_ok = atom_ok && fs_roa_delete(&tmp, "10.0.0.0/8") != FS_OK &&
                  read_block(&tmp, DATA_BITMAP_BLOCK, dbm10) == FS_OK &&
                  bitmap_test(dbm10, ino10.direct[0] - DATA_BLOCKS_START) &&
                  bitmap_test(dbm10, extra_blk - DATA_BLOCKS_START) &&
                  fs_read_superblock(&tmp, &sb10) == FS_OK && sb10.free_blocks == 50;
    } else {
        atom_ok = false;
    }
    if (atom_ok) {
        printf("[PASS] 10. Delete con inodo corrupto falla sin dejar bitmaps/superbloque a medias\n");
        passed++;
    } else {
        printf("[FAIL] 10. Delete con inodo corrupto deja el FS inconsistente\n");
        failed++;
    }

    printf("======================================================\n");
    printf("Resultado: %d aprobadas, %d fallidas\n", passed, failed);
    return (failed == 0) ? 0 : 1;
}

/* ========================================================================== */
/* Lectura segura de consola y menú interactivo                               */
/* ========================================================================== */

static bool read_line(char *buf, size_t size) {
    if (!fgets(buf, (int)size, stdin)) {
        return false;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        if (len >= 2 && buf[len - 2] == '\r') {
            buf[len - 2] = '\0';
        }
    } else {
        /* Descartar el resto de la línea si superó el tamaño del búfer */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
    }
    return true;
}

static bool read_int(const char *prompt, int *out_val) {
    char line[64];
    for (;;) {
        printf("%s", prompt);
        if (!read_line(line, sizeof(line))) {
            return false;
        }
        char *endptr = NULL;
        long val = strtol(line, &endptr, 10);
        while (endptr && isspace((unsigned char)*endptr)) {
            endptr++;
        }
        if (line[0] != '\0' && endptr && *endptr == '\0' &&
            val >= INT_MIN && val <= INT_MAX) {
            *out_val = (int)val;
            return true;
        }
        printf("Entrada inválida. Ingrese un número entero.\n");
    }
}

static void menu_put_roa(Disk *disk) {
    char ip[64];
    char owner[64];
    char roa_text[1024];

    printf("\n--- Guardar / Reemplazar ROA (put) ---\n");
    printf("IP o prefijo (ej. 192.168.1.0/24, máx %d caracteres): ", MAX_IP_LEN);
    if (!read_line(ip, sizeof(ip))) return;

    printf("Dueño / entidad (Enter para 'admin', máx %d caracteres): ", MAX_OWNER_LEN);
    if (!read_line(owner, sizeof(owner))) return;

    printf("Contenido del ROA (máx %d bytes): ", MAX_ROA_SIZE);
    if (!read_line(roa_text, sizeof(roa_text))) return;

    bool replaced = false;
    uint32_t len = (uint32_t)strlen(roa_text);
    int rc = fs_roa_put(disk, ip, owner, (const uint8_t *)roa_text, len, &replaced);
    if (rc != FS_OK) {
        printf("[ERROR] No se pudo guardar el ROA: %s\n", fs_strerror(rc));
        return;
    }

    Superblock sb;
    (void)fs_read_superblock(disk, &sb);
    printf("[OK] ROA %s correctamente para '%s' (%" PRIu32 " B). Nueva versión del FS: %" PRIu32 "\n",
           replaced ? "reemplazado" : "guardado", ip, len, sb.version);
}

static void menu_get_roa(Disk *disk) {
    char ip[64];
    printf("\n--- Consultar ROA (get) ---\n");
    printf("IP o prefijo a consultar: ");
    if (!read_line(ip, sizeof(ip))) return;

    uint8_t data[MAX_ROA_SIZE + 1];
    uint32_t size = 0;
    Inode ino;
    int ino_idx = -1;

    int rc = fs_roa_get(disk, ip, data, MAX_ROA_SIZE, &size, &ino, &ino_idx);
    if (rc != FS_OK) {
        printf("[ERROR] No se pudo consultar '%s': %s\n", ip, fs_strerror(rc));
        return;
    }

    data[size] = '\0';
    char c_ts[32], m_ts[32], a_ts[32];
    format_timestamp(ino.created_at, c_ts, sizeof(c_ts));
    format_timestamp(ino.modified_at, m_ts, sizeof(m_ts));
    format_timestamp(ino.accessed_at, a_ts, sizeof(a_ts));

    printf("\nROA encontrado:\n");
    printf("  IP           : %s\n", ip);
    printf("  Inodo        : %d\n", ino_idx);
    printf("  Dueño        : %s\n", ino.owner);
    printf("  Tamaño       : %" PRIu32 " bytes\n", size);
    printf("  Punteros     : directos=[%" PRId32 ", %" PRId32 ", %" PRId32 ", %" PRId32 "], indirecto=%" PRId32 "\n",
           ino.direct[0], ino.direct[1], ino.direct[2], ino.direct[3], ino.indirect);
    printf("  Creado       : %s\n", c_ts);
    printf("  Modificado   : %s\n", m_ts);
    printf("  Últ. acceso  : %s\n", a_ts);

    bool all_printable = true;
    for (uint32_t i = 0; i < size; ++i) {
        if (!isprint(data[i]) && data[i] != '\t' && data[i] != '\n') {
            all_printable = false;
            break;
        }
    }
    if (size == 0) {
        printf("  Contenido    : (vacío)\n");
    } else if (all_printable) {
        printf("  Contenido    : %s\n", (const char *)data);
    } else {
        printf("  Contenido (hex):\n");
        print_hexdump(data, size);
    }
}

static void menu_delete_roa(Disk *disk) {
    char ip[64];
    printf("\n--- Borrar ROA (delete) ---\n");
    printf("IP o prefijo a borrar: ");
    if (!read_line(ip, sizeof(ip))) return;

    int rc = fs_roa_delete(disk, ip);
    if (rc != FS_OK) {
        printf("[ERROR] No se pudo borrar '%s': %s\n", ip, fs_strerror(rc));
        return;
    }

    Superblock sb;
    (void)fs_read_superblock(disk, &sb);
    printf("[OK] ROA '%s' borrado correctamente. Nueva versión del FS: %" PRIu32 "\n",
           ip, sb.version);
}

static void menu_chs_lba(void) {
    int mode = 0;
    printf("\n--- Traducir CHS <-> LBA ---\n");
    printf("  1. CHS -> LBA\n");
    printf("  2. LBA -> CHS\n");
    if (!read_int("Seleccione dirección: ", &mode)) return;

    if (mode == 1) {
        int c = 0, h = 0, s = 0;
        if (!read_int("Cilindro / Pista (C, 0..15): ", &c)) return;
        if (!read_int("Plato / Cabeza   (H, 0..1) : ", &h)) return;
        if (!read_int("Sector           (S, 0..7) : ", &s)) return;
        int lba = chs_to_lba(c, h, s);
        if (lba < 0) {
            printf("[ERROR] %s\n", fs_strerror(lba));
            return;
        }
        int blk = lba / SECTORS_PER_BLOCK;
        int off = lba % SECTORS_PER_BLOCK;
        printf("[OK] CHS(%d, %d, %d) -> LBA %d (Bloque lógico %d [%s], sector interno %d/3)\n",
               c, h, s, lba, blk, block_region_name(blk), off);
    } else if (mode == 2) {
        int lba = 0, c = 0, h = 0, s = 0;
        if (!read_int("Sector LBA (0..255): ", &lba)) return;
        int rc = lba_to_chs(lba, &c, &h, &s);
        if (rc != FS_OK) {
            printf("[ERROR] %s\n", fs_strerror(rc));
            return;
        }
        int blk = lba / SECTORS_PER_BLOCK;
        int off = lba % SECTORS_PER_BLOCK;
        printf("[OK] LBA %d -> CHS(C=%d, H=%d, S=%d) (Bloque lógico %d [%s], sector interno %d/3)\n",
               lba, c, h, s, blk, block_region_name(blk), off);
    } else {
        printf("Opción no válida.\n");
    }
}

static void menu_read_sector(const Disk *disk) {
    int mode = 0;
    printf("\n--- Leer Sector (128 bytes) ---\n");
    printf("  1. Por CHS\n");
    printf("  2. Por LBA\n");
    if (!read_int("Modo de direccionamiento: ", &mode)) return;

    uint8_t buf[SECTOR_SIZE];
    int lba = -1, c = 0, h = 0, s = 0;
    int rc = FS_ERR_INVALID_ARG;

    if (mode == 1) {
        if (!read_int("Cilindro / Pista (0..15): ", &c)) return;
        if (!read_int("Plato / Cabeza   (0..1) : ", &h)) return;
        if (!read_int("Sector           (0..7) : ", &s)) return;
        lba = chs_to_lba(c, h, s);
        rc = read_sector_chs(disk, c, h, s, buf);
    } else if (mode == 2) {
        if (!read_int("LBA (0..255): ", &lba)) return;
        rc = lba_to_chs(lba, &c, &h, &s);
        if (rc == FS_OK) {
            rc = read_sector_lba(disk, lba, buf);
        }
    } else {
        printf("Opción no válida.\n");
        return;
    }

    if (rc != FS_OK) {
        printf("[ERROR] %s\n", fs_strerror(rc));
        return;
    }
    int blk = lba / SECTORS_PER_BLOCK;
    printf("Sector LBA %d | CHS(%d, %d, %d) | Bloque %d (%s):\n",
           lba, c, h, s, blk, block_region_name(blk));
    print_hexdump(buf, SECTOR_SIZE);
}

static void menu_write_sector(Disk *disk) {
    int mode = 0;
    printf("\n--- Escribir Sector (128 bytes) ---\n");
    printf("  1. Por CHS\n");
    printf("  2. Por LBA\n");
    if (!read_int("Modo de direccionamiento: ", &mode)) return;

    int lba = -1, c = 0, h = 0, s = 0;
    if (mode == 1) {
        if (!read_int("Cilindro / Pista (0..15): ", &c)) return;
        if (!read_int("Plato / Cabeza   (0..1) : ", &h)) return;
        if (!read_int("Sector           (0..7) : ", &s)) return;
        lba = chs_to_lba(c, h, s);
        if (lba < 0) {
            printf("[ERROR] %s\n", fs_strerror(lba));
            return;
        }
    } else if (mode == 2) {
        if (!read_int("LBA (0..255): ", &lba)) return;
        int rc = lba_to_chs(lba, &c, &h, &s);
        if (rc != FS_OK) {
            printf("[ERROR] %s\n", fs_strerror(rc));
            return;
        }
    } else {
        printf("Opción no válida.\n");
        return;
    }

    int blk = lba / SECTORS_PER_BLOCK;
    if (blk < DATA_BLOCKS_START) {
        printf("[AVISO] El sector LBA %d pertenece al bloque de metadatos %d (%s).\n",
               lba, blk, block_region_name(blk));
    }

    char text[256];
    printf("Texto a escribir (hasta %d B, el resto se rellena con ceros): ", SECTOR_SIZE);
    if (!read_line(text, sizeof(text))) return;

    uint8_t buf[SECTOR_SIZE];
    memset(buf, 0, sizeof(buf));
    size_t len = strlen(text);
    if (len > SECTOR_SIZE) {
        len = SECTOR_SIZE;
    }
    memcpy(buf, text, len);

    int rc = write_sector_chs(disk, c, h, s, buf);
    if (rc != FS_OK) {
        printf("[ERROR] %s\n", fs_strerror(rc));
        return;
    }
    printf("[OK] Escritos %zu bytes (+ relleno cero) en LBA %d / CHS(%d, %d, %d).\n",
           len, lba, c, h, s);
}

static void menu_read_block(const Disk *disk) {
    int blk = 0;
    printf("\n--- Leer Bloque (512 bytes) ---\n");
    if (!read_int("Número de bloque (0..63): ", &blk)) return;

    uint8_t buf[BLOCK_SIZE];
    int rc = read_block(disk, blk, buf);
    if (rc != FS_OK) {
        printf("[ERROR] %s\n", fs_strerror(rc));
        return;
    }
    printf("Bloque %d (%s) | Sectores LBA %d..%d:\n",
           blk, block_region_name(blk),
           blk * SECTORS_PER_BLOCK, blk * SECTORS_PER_BLOCK + SECTORS_PER_BLOCK - 1);
    print_hexdump(buf, BLOCK_SIZE);
}

static void menu_write_block(Disk *disk) {
    int blk = 0;
    printf("\n--- Escribir Bloque (512 bytes) ---\n");
    if (!read_int("Número de bloque (0..63): ", &blk)) return;
    if (blk < 0 || blk >= TOTAL_BLOCKS) {
        printf("[ERROR] %s\n", fs_strerror(FS_ERR_OUT_OF_RANGE));
        return;
    }
    if (blk < DATA_BLOCKS_START) {
        printf("[AVISO] El bloque %d pertenece a metadatos (%s). Sobrescribirlo puede alterar el FS.\n",
               blk, block_region_name(blk));
    }

    char text[1024];
    printf("Texto a escribir (hasta %d B, el resto se rellena con ceros): ", BLOCK_SIZE);
    if (!read_line(text, sizeof(text))) return;

    uint8_t buf[BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    size_t len = strlen(text);
    if (len > BLOCK_SIZE) {
        len = BLOCK_SIZE;
    }
    memcpy(buf, text, len);

    int rc = write_block(disk, blk, buf);
    if (rc != FS_OK) {
        printf("[ERROR] %s\n", fs_strerror(rc));
        return;
    }
    printf("[OK] Bloque %d (%s) actualizado (%zu bytes de texto + relleno cero).\n",
           blk, block_region_name(blk), len);
}

static void print_menu(void) {
    printf("\n============================================================\n");
    printf("        SIMULADOR DE DISCO Y SISTEMA DE ARCHIVOS (fsDisk)   \n");
    printf("============================================================\n");
    printf("  --- Operaciones ROA (Alto Nivel) ---\n");
    printf("   1. Guardar / Reemplazar ROA (put)\n");
    printf("   2. Consultar ROA (get)\n");
    printf("   3. Borrar ROA (delete)\n");
    printf("   4. Listar ROAs (list)\n");
    printf("  --- Estado y Administración ---\n");
    printf("   5. Ver estado del disco simulado\n");
    printf("   6. Formatear disco\n");
    printf("  --- Operaciones de Bajo Nivel ---\n");
    printf("   7. Traducir CHS <-> LBA\n");
    printf("   8. Leer sector (CHS / LBA)\n");
    printf("   9. Escribir sector (CHS / LBA)\n");
    printf("  10. Leer bloque (0..63)\n");
    printf("  11. Escribir bloque (0..63)\n");
    printf("  --- Verificación ---\n");
    printf("  12. Ejecutar pruebas automáticas (en disco temporal)\n");
    printf("   0. Salir\n");
    printf("============================================================\n");
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        return run_self_tests();
    }

    Disk disk;
    disk_init(&disk);
    if (fs_format(&disk) != FS_OK) {
        fprintf(stderr, "Error fatal al formatear el disco en memoria.\n");
        return 1;
    }

    printf("Disco simulado en memoria de 32 KB inicializado y formateado.\n");
    printf("Geometría: 2 platos x 16 pistas x 8 sectores x 128 B = 256 LBA (64 bloques de 512 B).\n");

    int option = -1;
    for (;;) {
        print_menu();
        if (!read_int("Seleccione una opción: ", &option)) {
            printf("\nFin de entrada. Saliendo...\n");
            break;
        }

        switch (option) {
            case 1:
                menu_put_roa(&disk);
                break;
            case 2:
                menu_get_roa(&disk);
                break;
            case 3:
                menu_delete_roa(&disk);
                break;
            case 4: {
                int rc = fs_roa_list(&disk, stdout);
                if (rc < 0) {
                    printf("[ERROR] %s\n", fs_strerror(rc));
                }
                break;
            }
            case 5:
                fs_print_status(&disk);
                break;
            case 6: {
                int rc = fs_format(&disk);
                if (rc == FS_OK) {
                    printf("[OK] Disco formateado correctamente (versión reiniciada a 0).\n");
                } else {
                    printf("[ERROR] %s\n", fs_strerror(rc));
                }
                break;
            }
            case 7:
                menu_chs_lba();
                break;
            case 8:
                menu_read_sector(&disk);
                break;
            case 9:
                menu_write_sector(&disk);
                break;
            case 10:
                menu_read_block(&disk);
                break;
            case 11:
                menu_write_block(&disk);
                break;
            case 12:
                (void)run_self_tests();
                break;
            case 0:
                printf("Saliendo del simulador fsDisk.\n");
                return 0;
            default:
                printf("Opción no reconocida. Intente de nuevo.\n");
                break;
        }
    }

    return 0;
}
