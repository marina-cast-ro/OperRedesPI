// Almacenamiento provisional en memoria, mientras llega el punto D

#include "repoStorage.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int used;
    char ip[16];
    char owner[OWNER_MAX_SIZE];
    char roa[ROA_MAX_SIZE];
} StoredRoa;

static StoredRoa entries[ROA_MAX_COUNT];
static long version = 0;

// Retorna la entrada de esa IP, o NULL si no hay
static StoredRoa *findEntry(const char *ip) {
    for (size_t i = 0; i < ROA_MAX_COUNT; i++) {
        if (entries[i].used && strcmp(entries[i].ip, ip) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

int storageInit(void) {
    memset(entries, 0, sizeof entries);
    version = 0;
    return STORAGE_OK;
}

int storagePut(const char *ip, const char *owner, const char *roa) {
    StoredRoa *entry = findEntry(ip);
    for (size_t i = 0; entry == NULL && i < ROA_MAX_COUNT; i++) {
        if (!entries[i].used) {
            entry = &entries[i];
        }
    }
    if (entry == NULL) {
        return STORAGE_FULL;
    }
    entry->used = 1;
    snprintf(entry->ip, sizeof entry->ip, "%s", ip);
    snprintf(entry->owner, sizeof entry->owner, "%s", owner);
    snprintf(entry->roa, sizeof entry->roa, "%s", roa);
    version++;
    return STORAGE_OK;
}

int storageGet(const char *ip, char *roa, char *owner) {
    StoredRoa *entry = findEntry(ip);
    if (entry == NULL) {
        return STORAGE_NOT_FOUND;
    }
    snprintf(roa, ROA_MAX_SIZE, "%s", entry->roa);
    snprintf(owner, OWNER_MAX_SIZE, "%s", entry->owner);
    return STORAGE_OK;
}

int storageDelete(const char *ip) {
    StoredRoa *entry = findEntry(ip);
    if (entry == NULL) {
        return STORAGE_NOT_FOUND;
    }
    entry->used = 0;
    version++;
    return STORAGE_OK;
}

size_t storageList(char roas[][ROA_MAX_SIZE], size_t maxRoas) {
    size_t count = 0;
    for (size_t i = 0; i < ROA_MAX_COUNT && count < maxRoas; i++) {
        if (entries[i].used) {
            snprintf(roas[count], ROA_MAX_SIZE, "%s", entries[i].roa);
            count++;
        }
    }
    return count;
}

long storageVersion(void) {
    return version;
}
