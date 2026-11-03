#ifndef REPO_STORAGE_H
#define REPO_STORAGE_H

// Lo que el servidor le pide al punto D. Mientras tanto las implementa repoStorageMemory.c
// El servidor las llama con un candado de lectores y escritores tomado

#include <stddef.h>

// Un ROA va en un bloque de 512 bytes, relleno con \0
#define ROA_MAX_SIZE 512
// La tabla de inodos tiene 32
#define ROA_MAX_COUNT 32
// CN del certificado, por ejemplo AS65002-201
#define OWNER_MAX_SIZE 64

#define STORAGE_OK 0
#define STORAGE_FULL (-1)       // ERR_LLENO
#define STORAGE_NOT_FOUND (-2)  // ERR_NO_EXISTE

// Prepara el disco. Retorna STORAGE_OK, o -1 si no se pudo
int storageInit(void);

// Guarda o reemplaza el ROA de esa IP y sube la versión. Retorna STORAGE_OK o STORAGE_FULL
int storagePut(const char *ip, const char *owner, const char *roa);

// Copia el ROA de esa IP y su dueño. Retorna STORAGE_OK o STORAGE_NOT_FOUND
int storageGet(const char *ip, char *roa, char *owner);

// Borra el ROA de esa IP y sube la versión. Retorna STORAGE_OK o STORAGE_NOT_FOUND
int storageDelete(const char *ip);

// Copia todos los ROAs en el arreglo. Retorna cuántos copió
size_t storageList(char roas[][ROA_MAX_SIZE], size_t maxRoas);

// Contador de generación del superbloque. Empieza en 0
long storageVersion(void);

#endif // REPO_STORAGE_H
