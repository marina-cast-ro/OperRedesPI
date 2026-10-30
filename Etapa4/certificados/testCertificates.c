// Prueba del módulo de certificados (punto A).
// Uso: ./testCertificates <trusted-roots.pem> <certificado.crt> [<ISLA> <NUM_EMISOR>]
//
// Hace con el certificado lo mismo que el repositorio o un enrutador con uno que llegó por la red:
// lo pasa a base64 y de vuelta (como viaja en HELLO y en CERT), lo valida contra el truststore
// y muestra los datos que usa el punto B. Termina con 0 si el certificado es válido y 1 si no.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "certificates.h"

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 5) {
        fprintf(stderr, "Uso: %s <trusted-roots.pem> <certificado.crt> [<ISLA> <NUM_EMISOR>]\n", argv[0]);
        return 2;
    }

    X509_STORE *store = loadTrustStore(argv[1]);
    if (store == NULL) {
        fprintf(stderr, "No se pudo cargar el truststore %s\n", argv[1]);
        return 2;
    }
    X509 *original = certFromFile(argv[2]);
    if (original == NULL) {
        fprintf(stderr, "No se pudo leer el certificado %s\n", argv[2]);
        X509_STORE_free(store);
        return 2;
    }

    // Ida y vuelta por base64, como si el certificado hubiera llegado en un mensaje
    char *base64 = certToBase64(original);
    X509 *cert = (base64 != NULL) ? certFromBase64(base64) : NULL;
    if (cert == NULL) {
        fprintf(stderr, "Falló la conversión a base64 y de vuelta\n");
        free(base64);
        X509_free(original);
        X509_STORE_free(store);
        return 2;
    }
    printf("Base64:      %zu caracteres, empieza con %.24s...\n", strlen(base64), base64);

    char cn[64];
    if (getCommonName(cert, cn, sizeof cn) == 0) {
        printf("CN:          %s\n", cn);
    } else {
        printf("CN:          (no tiene)\n");
    }

    time_t notAfter = getNotAfter(cert);
    char date[32];
    strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", gmtime(&notAfter));
    printf("notAfter:    %ld (%s UTC)\n", (long)notAfter, date);
    printf("Llave:       RSA de %d bits\n", EVP_PKEY_get_bits(getPublicKey(cert)));

    if (argc == 5) {
        int island = atoi(argv[3]);
        int emitterNum = atoi(argv[4]);
        printf("CN = AS%d-%d: %s\n", island, emitterNum, cnMatches(cert, island, emitterNum) ? "sí" : "no");
    }

    char reason[256];
    int valid = validateNodeCert(store, cert, reason, sizeof reason);
    if (valid) {
        printf("Resultado:   VÁLIDO\n");
    } else {
        printf("Resultado:   RECHAZADO, %s\n", reason);
    }

    free(base64);
    X509_free(cert);
    X509_free(original);
    X509_STORE_free(store);
    return valid ? 0 : 1;
}
