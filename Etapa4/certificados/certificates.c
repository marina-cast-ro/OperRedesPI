#include "certificates.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

X509_STORE *loadTrustStore(const char *path) {
    X509_STORE *store = X509_STORE_new();
    if (store == NULL) {
        return NULL;
    }
    // Carga todos los certificados del archivo: las raíces de los grupos van una detrás de otra
    if (X509_STORE_load_file(store, path) != 1) {
        X509_STORE_free(store);
        return NULL;
    }
    return store;
}

X509 *certFromFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }
    X509 *cert = PEM_read_X509(file, NULL, NULL, NULL);
    fclose(file);
    return cert;
}

char *certToBase64(X509 *cert) {
    // i2d_X509 pasa el certificado a binario (DER) y reserva el buffer
    unsigned char *der = NULL;
    int derLen = i2d_X509(cert, &der);
    if (derLen <= 0) {
        return NULL;
    }
    // Cada 3 bytes se vuelven 4 caracteres, más el \0 del final
    char *base64 = malloc((size_t)(4 * ((derLen + 2) / 3) + 1));
    if (base64 != NULL) {
        // EVP_EncodeBlock escribe todo en una línea, sin saltos
        EVP_EncodeBlock((unsigned char *)base64, der, derLen);
    }
    OPENSSL_free(der);
    return base64;
}

X509 *certFromBase64(const char *base64) {
    // Lo que llega por la red puede traer el \n o \r\n del final de la línea
    size_t len = strlen(base64);
    while (len > 0 && (base64[len - 1] == '\n' || base64[len - 1] == '\r' || base64[len - 1] == ' ')) {
        len--;
    }
    if (len == 0 || len % 4 != 0) {
        return NULL;
    }

    unsigned char *der = malloc(len / 4 * 3);
    if (der == NULL) {
        return NULL;
    }
    int derLen = EVP_DecodeBlock(der, (const unsigned char *)base64, (int)len);
    // EVP_DecodeBlock cuenta los '=' del relleno como bytes: se descuentan
    if (derLen > 0 && base64[len - 1] == '=') {
        derLen--;
    }
    if (derLen > 0 && base64[len - 2] == '=') {
        derLen--;
    }

    X509 *cert = NULL;
    if (derLen > 0) {
        const unsigned char *cursor = der;
        cert = d2i_X509(NULL, &cursor, derLen);
        // Si sobraron bytes después del certificado, el texto no era solo un certificado
        if (cert != NULL && cursor != der + derLen) {
            X509_free(cert);
            cert = NULL;
        }
    }
    free(der);
    return cert;
}

// Revisa que el certificado tenga la extensión nid y que esté marcada como crítica
static int isCritical(X509 *cert, int nid) {
    int position = X509_get_ext_by_NID(cert, nid, -1);
    if (position < 0) {
        return 0;
    }
    return X509_EXTENSION_get_critical(X509_get_ext(cert, position));
}

int validateNodeCert(X509_STORE *store, X509 *cert, char *reason, size_t reasonSize) {
    if (store == NULL || cert == NULL) {
        snprintf(reason, reasonSize, "falta el truststore o el certificado");
        return 0;
    }

    X509_STORE_CTX *context = X509_STORE_CTX_new();
    if (context == NULL || X509_STORE_CTX_init(context, store, cert, NULL) != 1) {
        snprintf(reason, reasonSize, "no se pudo preparar la verificación");
        X509_STORE_CTX_free(context);
        return 0;
    }

    // Pasos 1 a 3 del diseño: OpenSSL revisa las fechas del nodo y de la raíz con la hora actual,
    // que el issuer del nodo sea una raíz del truststore y que la firma del certificado verifique
    if (X509_verify_cert(context) != 1) {
        int error = X509_STORE_CTX_get_error(context);
        snprintf(reason, reasonSize, "cadena no válida: %s", X509_verify_cert_error_string(error));
        X509_STORE_CTX_free(context);
        return 0;
    }

    // PKI plana: la cadena tiene que ser exactamente nodo -> raíz. Si mide 1, mandaron la raíz
    // como si fuera un nodo. Si mide más, hay una CA intermedia
    STACK_OF(X509) *chain = X509_STORE_CTX_get0_chain(context);
    int chainLen = sk_X509_num(chain);
    if (chainLen != 2) {
        snprintf(reason, reasonSize, "la cadena tiene %d certificados y tiene que ser nodo -> raíz", chainLen);
        X509_STORE_CTX_free(context);
        return 0;
    }
    X509 *root = sk_X509_value(chain, 1);

    // Paso 4: extensiones correctas y críticas
    int valid = 0;
    if (!isCritical(root, NID_basic_constraints) || X509_check_ca(root) != 1) {
        snprintf(reason, reasonSize, "la raíz no tiene CA:TRUE crítico");
    } else if (!isCritical(root, NID_key_usage) || !(X509_get_key_usage(root) & KU_KEY_CERT_SIGN)) {
        snprintf(reason, reasonSize, "la raíz no tiene keyCertSign crítico");
    } else if (!isCritical(cert, NID_basic_constraints) || X509_check_ca(cert) != 0) {
        snprintf(reason, reasonSize, "el nodo no tiene CA:FALSE crítico");
    } else if (!isCritical(cert, NID_key_usage) || !(X509_get_key_usage(cert) & KU_DIGITAL_SIGNATURE)) {
        snprintf(reason, reasonSize, "el nodo no tiene digitalSignature crítico");
    } else {
        valid = 1;
    }

    X509_STORE_CTX_free(context);
    return valid;
}

int getCommonName(X509 *cert, char *cn, size_t cnSize) {
    X509_NAME *subject = X509_get_subject_name(cert);
    int position = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
    if (position < 0) {
        return -1;
    }
    ASN1_STRING *data = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subject, position));
    int len = ASN1_STRING_length(data);
    if (len < 0 || (size_t)len >= cnSize) {
        return -1;
    }
    memcpy(cn, ASN1_STRING_get0_data(data), (size_t)len);
    cn[len] = '\0';
    return 0;
}

int cnMatches(X509 *cert, int island, int emitterNum) {
    char expected[64];
    char cn[64];
    snprintf(expected, sizeof expected, "AS%d-%d", island, emitterNum);
    if (getCommonName(cert, cn, sizeof cn) != 0) {
        return 0;
    }
    return strcmp(cn, expected) == 0;
}

time_t getNotAfter(X509 *cert) {
    // Diferencia entre el 1-ene-1970 (segundo 0 de Unix) y el notAfter, en días y segundos
    ASN1_TIME *epoch = ASN1_TIME_set(NULL, 0);
    if (epoch == NULL) {
        return -1;
    }
    int days = 0;
    int seconds = 0;
    int ok = ASN1_TIME_diff(&days, &seconds, epoch, X509_get0_notAfter(cert));
    ASN1_TIME_free(epoch);
    if (!ok) {
        return -1;
    }
    return (time_t)days * 86400 + seconds;
}

EVP_PKEY *getPublicKey(X509 *cert) {
    return X509_get0_pubkey(cert);
}
