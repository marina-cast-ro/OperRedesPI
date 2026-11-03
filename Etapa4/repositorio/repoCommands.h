#ifndef REPO_COMMANDS_H
#define REPO_COMMANDS_H

#include <stddef.h>
#include <openssl/x509.h>

#include "repoStorage.h"

// Un certificado en base64 ocupa unos 2 KB
#define MAX_LINE 16384
// La respuesta más larga es la de LIST
#define MAX_RESPONSE (MAX_LINE + ROA_MAX_COUNT * ROA_MAX_SIZE)
// 16 bytes en hexadecimal, más el \0
#define NONCE_SIZE 33

// Estado de una conexión
typedef struct {
    int greeted;                 // 1 si ya hizo HELLO
    int isOwner;                 // 1 si es OWNER, 0 si es ENRUTADOR
    int emitterNum;              // NUM_EMISOR del HELLO
    char owner[OWNER_MAX_SIZE];  // CN del certificado
    char nonce[NONCE_SIZE];      // Lo que tiene que firmar
    X509 *cert;                  // Certificado del HELLO
} Session;

// Carga las raíces de confianza y prepara el almacenamiento. Retorna 0, o -1 si no se pudo
int commandsInit(const char *trustStorePath);

// Recarga las raíces si el archivo cambió
void commandsRefreshTrustStore(void);

// Libera lo de commandsInit y los certificados guardados
void commandsFree(void);

void sessionInit(Session *session);
void sessionFree(Session *session);

// Deja en response lo que hay que contestar a la línea
void handleLine(Session *session, const char *line, char *response, size_t responseSize);

#endif // REPO_COMMANDS_H
