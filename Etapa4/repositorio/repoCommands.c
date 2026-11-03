#include "repoCommands.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "certificates.h"

// PUT es la línea con más campos: PUT, los 7 del ROA y la firma
#define MAX_FIELDS 9
#define ROA_FIELDS 7
// Un certificado por emisor
#define MAX_CERTS 64

// ROA|IP|ISLA|NUM_EMISOR|EMISION|VENCE|FIRMA
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int island;
    int emitterNum;
    long issuedAt;
    long expiresAt;
    char signedText[ROA_MAX_SIZE];  // Lo que firmó el nodo
    char signature[ROA_MAX_SIZE];   // En base64
} Roa;

// Certificado de un emisor, para contestar CERT
typedef struct {
    int used;
    int emitterNum;
    char owner[OWNER_MAX_SIZE];
    char *base64;
} KnownCert;

static KnownCert knownCerts[MAX_CERTS];

// Protege el almacenamiento y knownCerts
static pthread_rwlock_t storageLock;

// Protege trustStore
static pthread_rwlock_t trustLock;
static X509_STORE *trustStore = NULL;
static time_t trustStoreModified = 0;
static const char *trustStoreFile = NULL;

static void reply(char *response, size_t responseSize, const char *text) {
    snprintf(response, responseSize, "%s\n", text);
}

// Separa el texto por '|'. Retorna cuántos campos encontró, o maxFields + 1 si hay de más
static int splitFields(char *text, char *fields[], int maxFields) {
    int count = 0;
    char *cursor = text;
    while (count < maxFields) {
        fields[count++] = cursor;
        cursor = strchr(cursor, '|');
        if (cursor == NULL) {
            return count;
        }
        *cursor = '\0';
        cursor++;
    }
    return maxFields + 1;
}

// Retorna 0 si pudo convertir el número, -1 si trae algo que no sea un dígito
static int parseNumber(const char *text, long *value) {
    if (text[0] == '\0') {
        return -1;
    }
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return -1;
        }
    }
    errno = 0;
    *value = strtol(text, NULL, 10);
    return errno == 0 ? 0 : -1;
}

// Igual que parseNumber, para la isla y el número de emisor
static int parseInt(const char *text, int *value) {
    long number = 0;
    if (parseNumber(text, &number) != 0 || number > INT_MAX) {
        return -1;
    }
    *value = (int)number;
    return 0;
}

static int isValidIp(const char *text) {
    struct in_addr address;
    return inet_pton(AF_INET, text, &address) == 1;
}

// Separa un ROA en sus 7 campos. Retorna 0, o -1 si está mal formado
static int parseRoa(const char *text, Roa *roa) {
    char copy[ROA_MAX_SIZE];
    if (strlen(text) >= sizeof copy) {
        return -1;
    }
    strcpy(copy, text);

    char *fields[ROA_FIELDS];
    if (splitFields(copy, fields, ROA_FIELDS) != ROA_FIELDS || strcmp(fields[0], "ROA") != 0) {
        return -1;
    }
    if (!isValidIp(fields[1])
        || parseInt(fields[2], &roa->island) != 0
        || parseInt(fields[3], &roa->emitterNum) != 0
        || parseNumber(fields[4], &roa->issuedAt) != 0
        || parseNumber(fields[5], &roa->expiresAt) != 0
        || fields[6][0] == '\0') {
        return -1;
    }
    snprintf(roa->ip, sizeof roa->ip, "%s", fields[1]);
    snprintf(roa->signature, sizeof roa->signature, "%s", fields[6]);

    // Lo firmado es la línea hasta VENCE, sin el último '|'
    size_t signedLen = (size_t)(fields[6] - copy) - 1;
    memcpy(roa->signedText, text, signedLen);
    roa->signedText[signedLen] = '\0';
    return 0;
}

// Revisa una firma en base64 (RSA-2048 con SHA-256) sobre el texto. Retorna 1 si verifica, 0 si no
static int signatureVerifies(X509 *cert, const char *text, const char *signatureBase64) {
    size_t base64Len = strlen(signatureBase64);
    if (base64Len == 0 || base64Len % 4 != 0) {
        return 0;
    }
    unsigned char *signature = malloc(base64Len / 4 * 3);
    if (signature == NULL) {
        return 0;
    }
    int signatureLen = EVP_DecodeBlock(signature, (const unsigned char *)signatureBase64, (int)base64Len);
    // EVP_DecodeBlock cuenta los '=' como bytes
    if (signatureLen > 0 && signatureBase64[base64Len - 1] == '=') {
        signatureLen--;
    }
    if (signatureLen > 0 && signatureBase64[base64Len - 2] == '=') {
        signatureLen--;
    }

    int valid = 0;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (signatureLen > 0 && context != NULL
        && EVP_DigestVerifyInit(context, NULL, EVP_sha256(), NULL, getPublicKey(cert)) == 1) {
        valid = EVP_DigestVerify(context, signature, (size_t)signatureLen,
                                 (const unsigned char *)text, strlen(text)) == 1;
    }
    EVP_MD_CTX_free(context);
    free(signature);
    return valid;
}

// Retorna la casilla de ese emisor, o una libre si todavía no tiene. NULL si no hay espacio
static KnownCert *certSlotFor(int emitterNum) {
    KnownCert *freeSlot = NULL;
    for (size_t i = 0; i < MAX_CERTS; i++) {
        if (knownCerts[i].used && knownCerts[i].emitterNum == emitterNum) {
            return &knownCerts[i];
        }
        if (!knownCerts[i].used && freeSlot == NULL) {
            freeSlot = &knownCerts[i];
        }
    }
    return freeSlot;
}

// HELLO|ENRUTADOR|<id> o HELLO|OWNER|<num_emisor>|<cert_b64>
static void handleHello(Session *session, char *fields[], int count, char *response, size_t responseSize) {
    if (count >= 3 && count <= 4 && strcmp(fields[1], "ENRUTADOR") == 0) {
        session->greeted = 1;
        reply(response, responseSize, "HELLO_OK|ENRUTADOR|-");
        return;
    }
    if (count != 4 || strcmp(fields[1], "OWNER") != 0 || parseInt(fields[2], &session->emitterNum) != 0) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }
    session->cert = certFromBase64(fields[3]);
    if (session->cert == NULL) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }

    char reason[160];
    pthread_rwlock_rdlock(&trustLock);
    int trusted = validateNodeCert(trustStore, session->cert, reason, sizeof reason);
    pthread_rwlock_unlock(&trustLock);
    if (!trusted) {
        printf("Certificado rechazado: %s\n", reason);
        reply(response, responseSize, "ERR_PERMISO");
        return;
    }

    // El CN tiene que ser de ese emisor: AS<ISLA>-<NUM_EMISOR>
    long cnNum = 0;
    const char *dash = NULL;
    if (getCommonName(session->cert, session->owner, sizeof session->owner) == 0) {
        dash = strrchr(session->owner, '-');
    }
    if (dash == NULL || strncmp(session->owner, "AS", 2) != 0
        || parseNumber(dash + 1, &cnNum) != 0 || cnNum != session->emitterNum) {
        reply(response, responseSize, "ERR_PERMISO");
        return;
    }

    unsigned char randomBytes[(NONCE_SIZE - 1) / 2];
    if (RAND_bytes(randomBytes, sizeof randomBytes) != 1) {
        reply(response, responseSize, "ERR_PERMISO");
        return;
    }
    for (size_t i = 0; i < sizeof randomBytes; i++) {
        snprintf(&session->nonce[i * 2], 3, "%02x", randomBytes[i]);
    }

    session->isOwner = 1;
    session->greeted = 1;
    snprintf(response, responseSize, "HELLO_OK|OWNER|%s\n", session->nonce);
}

static void handleVersion(char *response, size_t responseSize) {
    pthread_rwlock_rdlock(&storageLock);
    long version = storageVersion();
    pthread_rwlock_unlock(&storageLock);
    snprintf(response, responseSize, "OK|%ld\n", version);
}

// Con un solo candado, para que todos los ROAs sean de una misma versión
static void handleList(char *response, size_t responseSize) {
    char roas[ROA_MAX_COUNT][ROA_MAX_SIZE];
    pthread_rwlock_rdlock(&storageLock);
    long version = storageVersion();
    size_t count = storageList(roas, ROA_MAX_COUNT);
    pthread_rwlock_unlock(&storageLock);

    size_t used = (size_t)snprintf(response, responseSize, "OK|%ld|%zu\n", version, count);
    for (size_t i = 0; i < count; i++) {
        used += (size_t)snprintf(response + used, responseSize - used, "%s\n", roas[i]);
    }
}

static void handleGet(const char *ip, char *response, size_t responseSize) {
    // En el Doc aparece como GET| <ip>, con espacio
    while (*ip == ' ') {
        ip++;
    }
    if (!isValidIp(ip)) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }
    char roa[ROA_MAX_SIZE];
    char owner[OWNER_MAX_SIZE];
    pthread_rwlock_rdlock(&storageLock);
    int result = storageGet(ip, roa, owner);
    pthread_rwlock_unlock(&storageLock);
    if (result != STORAGE_OK) {
        reply(response, responseSize, "ERR_NO_EXISTE");
        return;
    }
    snprintf(response, responseSize, "OK|%s\n", roa);
}

static void handleCert(const char *numText, char *response, size_t responseSize) {
    int emitterNum = 0;
    if (parseInt(numText, &emitterNum) != 0) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }
    reply(response, responseSize, "ERR_NO_EXISTE");
    pthread_rwlock_rdlock(&storageLock);
    KnownCert *known = certSlotFor(emitterNum);
    if (known != NULL && known->used) {
        snprintf(response, responseSize, "OK|%s\n", known->base64);
    }
    pthread_rwlock_unlock(&storageLock);
}

// PUT|<roa>|<firma_b64>, con la firma sobre PUT|<roa>|<nonce>
static void handlePut(Session *session, const char *line, char *response, size_t responseSize) {
    const char *roaStart = line + strlen("PUT|");
    const char *lastPipe = strrchr(line, '|');
    size_t roaLen = (size_t)(lastPipe - roaStart);
    if (roaLen >= ROA_MAX_SIZE) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }
    char roaText[ROA_MAX_SIZE];
    memcpy(roaText, roaStart, roaLen);
    roaText[roaLen] = '\0';

    Roa roa;
    if (parseRoa(roaText, &roa) != 0) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }

    char signedRequest[ROA_MAX_SIZE + NONCE_SIZE + 8];
    snprintf(signedRequest, sizeof signedRequest, "PUT|%s|%s", roaText, session->nonce);
    if (!signatureVerifies(session->cert, signedRequest, lastPipe + 1)) {
        reply(response, responseSize, "ERR_FIRMA");
        return;
    }

    // El ROA tiene que ser de quien lo manda
    if (roa.emitterNum != session->emitterNum || !cnMatches(session->cert, roa.island, roa.emitterNum)) {
        reply(response, responseSize, "ERR_PERMISO");
        return;
    }
    if (!signatureVerifies(session->cert, roa.signedText, roa.signature)) {
        reply(response, responseSize, "ERR_FIRMA");
        return;
    }
    long now = (long)time(NULL);
    if (now < roa.issuedAt || now > roa.expiresAt || roa.expiresAt > (long)getNotAfter(session->cert)) {
        reply(response, responseSize, "ERR_EXPIRADO");
        return;
    }

    char *certBase64 = certToBase64(session->cert);
    if (certBase64 == NULL) {
        reply(response, responseSize, "ERR_LLENO");
        return;
    }

    pthread_rwlock_wrlock(&storageLock);
    char storedRoa[ROA_MAX_SIZE];
    char storedOwner[OWNER_MAX_SIZE];
    int exists = storageGet(roa.ip, storedRoa, storedOwner) == STORAGE_OK;
    KnownCert *known = certSlotFor(roa.emitterNum);

    if (known == NULL) {
        reply(response, responseSize, "ERR_LLENO");
    } else if ((known->used && strcmp(known->owner, session->owner) != 0)
               || (exists && strcmp(storedOwner, session->owner) != 0)) {
        // Otra isla ya usa ese NUM_EMISOR, o esa IP tiene el ROA de otro dueño
        reply(response, responseSize, "ERR_PERMISO");
    } else if (!(exists && strcmp(storedRoa, roaText) == 0)
               && storagePut(roa.ip, session->owner, roaText) != STORAGE_OK) {
        reply(response, responseSize, "ERR_LLENO");
    } else {
        // Un PUT idéntico no llama a storagePut, así que no sube la versión
        free(known->base64);
        known->used = 1;
        known->emitterNum = roa.emitterNum;
        snprintf(known->owner, sizeof known->owner, "%s", session->owner);
        known->base64 = certBase64;
        certBase64 = NULL;
        snprintf(response, responseSize, "OK|%ld\n", storageVersion());
    }
    pthread_rwlock_unlock(&storageLock);
    free(certBase64);
}

// DELETE|<ip>|<firma_b64>, con la firma sobre DELETE|<ip>|<nonce>
static void handleDelete(Session *session, const char *ip, const char *signature, char *response, size_t responseSize) {
    if (!isValidIp(ip)) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }
    char signedRequest[INET_ADDRSTRLEN + NONCE_SIZE + 16];
    snprintf(signedRequest, sizeof signedRequest, "DELETE|%s|%s", ip, session->nonce);
    if (!signatureVerifies(session->cert, signedRequest, signature)) {
        reply(response, responseSize, "ERR_FIRMA");
        return;
    }

    pthread_rwlock_wrlock(&storageLock);
    char storedRoa[ROA_MAX_SIZE];
    char storedOwner[OWNER_MAX_SIZE];
    if (storageGet(ip, storedRoa, storedOwner) != STORAGE_OK) {
        reply(response, responseSize, "ERR_NO_EXISTE");
    } else if (strcmp(storedOwner, session->owner) != 0) {
        reply(response, responseSize, "ERR_PERMISO");
    } else {
        storageDelete(ip);
        snprintf(response, responseSize, "OK|%ld\n", storageVersion());
    }
    pthread_rwlock_unlock(&storageLock);
}

static int isCommand(const char *command) {
    return strcmp(command, "VERSION") == 0 || strcmp(command, "LIST") == 0
        || strcmp(command, "GET") == 0 || strcmp(command, "CERT") == 0
        || strcmp(command, "PUT") == 0 || strcmp(command, "DELETE") == 0;
}

void handleLine(Session *session, const char *line, char *response, size_t responseSize) {
    char copy[MAX_LINE];
    if (strlen(line) >= sizeof copy) {
        reply(response, responseSize, "ERR_FORMATO");
        return;
    }
    strcpy(copy, line);
    char *fields[MAX_FIELDS];
    int count = splitFields(copy, fields, MAX_FIELDS);
    const char *command = fields[0];

    if (!session->greeted) {
        if (strcmp(command, "HELLO") == 0) {
            handleHello(session, fields, count, response, responseSize);
        } else {
            reply(response, responseSize, isCommand(command) ? "ERR_SIN_HELLO" : "ERR_FORMATO");
        }
        return;
    }

    // Un enrutador no puede escribir
    if ((strcmp(command, "PUT") == 0 || strcmp(command, "DELETE") == 0) && !session->isOwner) {
        reply(response, responseSize, "ERR_PERMISO");
    } else if (strcmp(command, "VERSION") == 0 && count == 1) {
        handleVersion(response, responseSize);
    } else if (strcmp(command, "LIST") == 0 && count == 1) {
        handleList(response, responseSize);
    } else if (strcmp(command, "GET") == 0 && count == 2) {
        handleGet(fields[1], response, responseSize);
    } else if (strcmp(command, "CERT") == 0 && count == 2) {
        handleCert(fields[1], response, responseSize);
    } else if (strcmp(command, "PUT") == 0 && count == 1 + ROA_FIELDS + 1) {
        handlePut(session, line, response, responseSize);
    } else if (strcmp(command, "DELETE") == 0 && count == 3) {
        handleDelete(session, fields[1], fields[2], response, responseSize);
    } else {
        reply(response, responseSize, "ERR_FORMATO");
    }
}

void sessionInit(Session *session) {
    memset(session, 0, sizeof *session);
}

void sessionFree(Session *session) {
    X509_free(session->cert);
    session->cert = NULL;
}

// Prepara un candado con preferencia a escritores, para que muchos LIST no dejen esperando a un PUT
static int initLock(pthread_rwlock_t *lock) {
    pthread_rwlockattr_t attributes;
    if (pthread_rwlockattr_init(&attributes) != 0) {
        return -1;
    }
    pthread_rwlockattr_setkind_np(&attributes, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    int result = pthread_rwlock_init(lock, &attributes);
    pthread_rwlockattr_destroy(&attributes);
    return result;
}

int commandsInit(const char *trustStorePath) {
    if (initLock(&storageLock) != 0 || initLock(&trustLock) != 0) {
        return -1;
    }
    trustStoreFile = trustStorePath;
    if (reloadTrustStoreIfChanged(trustStoreFile, &trustStore, &trustStoreModified) != 1) {
        return -1;
    }
    return storageInit() == STORAGE_OK ? 0 : -1;
}

void commandsRefreshTrustStore(void) {
    pthread_rwlock_wrlock(&trustLock);
    int result = reloadTrustStoreIfChanged(trustStoreFile, &trustStore, &trustStoreModified);
    pthread_rwlock_unlock(&trustLock);
    if (result == 1) {
        printf("Raíces de confianza recargadas desde %s\n", trustStoreFile);
    } else if (result == -1) {
        printf("No se pudo leer %s: se conservan las raíces anteriores\n", trustStoreFile);
    }
}

void commandsFree(void) {
    X509_STORE_free(trustStore);
    trustStore = NULL;
    for (size_t i = 0; i < MAX_CERTS; i++) {
        free(knownCerts[i].base64);
        knownCerts[i].base64 = NULL;
        knownCerts[i].used = 0;
    }
}
