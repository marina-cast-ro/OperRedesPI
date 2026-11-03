#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#define TIMEOUT_SEGUNDOS   10
#define BUFFER_MAX         8192

// Codifica datos binarios a Base64 en una sola linea (sin saltos \n)
static int base64_encode(const unsigned char *input, size_t len, char *out, size_t max_out) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *mem = BIO_new(BIO_s_mem());
    BIO *bio = BIO_push(b64, mem);

    BIO_write(bio, input, (int)len);
    BIO_flush(bio);

    BUF_MEM *memPtr;
    BIO_get_mem_ptr(bio, &memPtr);

    if (memPtr->length >= max_out) {
        BIO_free_all(bio);
        return -1;
    }
    memcpy(out, memPtr->data, memPtr->length);
    out[memPtr->length] = '\0';

    BIO_free_all(bio);
    return 0;
}

// Firma un texto con la llave privada usando RSA-2048 y SHA-256
static int firmar_texto(EVP_PKEY *pkey, const char *texto, char *b64_out, size_t max_b64) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return -1;

    if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, pkey) <= 0 ||
        EVP_DigestSignUpdate(ctx, texto, strlen(texto)) <= 0) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    size_t siglen = 0;
    if (EVP_DigestSignFinal(ctx, NULL, &siglen) <= 0) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    unsigned char *sig = malloc(siglen);
    if (!sig) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    if (EVP_DigestSignFinal(ctx, sig, &siglen) <= 0) {
        free(sig);
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);

    int ok = base64_encode(sig, siglen, b64_out, max_b64);
    free(sig);
    return ok;
}

// Carga el archivo .crt, extrae el DER y lo pasa a Base64
static int cargar_cert_base64(const char *ruta_cert, char *b64_out, size_t max_len) {
    FILE *f = fopen(ruta_cert, "r");
    if (!f) return -1;

    X509 *cert = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    if (!cert) return -1;

    int der_len = i2d_X509(cert, NULL);
    if (der_len <= 0) {
        X509_free(cert);
        return -1;
    }
    unsigned char *der = malloc(der_len);
    unsigned char *p = der;
    i2d_X509(cert, &p);

    int r = base64_encode(der, (size_t)der_len, b64_out, max_len);
    free(der);
    X509_free(cert);
    return r;
}

// Carga la llave privada .key (formato PEM)
static EVP_PKEY *cargar_llave_privada(const char *ruta_key) {
    FILE *f = fopen(ruta_key, "r");
    if (!f) return NULL;
    EVP_PKEY *pkey = PEM_read_PrivateKey(f, NULL, NULL, NULL);
    fclose(f);
    return pkey;
}

// Abre conexion TCP con timeout estricto de 10s usando poll
static int conectar_con_timeout(const char *ip, int puerto) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    // Conectar en modo no bloqueante
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)puerto);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int r = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (r < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }

    if (r < 0) {
        struct pollfd pfd = { .fd = sock, .events = POLLOUT };
        int pr = poll(&pfd, 1, TIMEOUT_SEGUNDOS * 1000);
        if (pr <= 0) {
            close(sock);
            return -1;
        }
        int err = 0;
        socklen_t sl = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &sl);
        if (err != 0) {
            close(sock);
            return -1;
        }
    }

    // Configurar timeouts de lectura y escritura a 10s
    struct timeval tv = { .tv_sec = TIMEOUT_SEGUNDOS, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Regresar a modo bloqueante
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
    return sock;
}

// Lee una linea hasta el salto de linea \n
static int leer_linea(int sock, char *buffer, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1) {
        char c;
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buffer[i++] = c;
    }
    buffer[i] = '\0';
    return (int)i;
}

// Flujo completo para publicar un ROA hacia el repositorio
int publicar_roa(const char *host, int puerto, int num_emisor, int isla, const char *ip_anunciada,
                  const char *ruta_key, const char *ruta_cert, long duracion_seg) {
    // 1. Cargar llaves y certificado
    EVP_PKEY *pkey = cargar_llave_privada(ruta_key);
    if (!pkey) {
        fprintf(stderr, "[ERROR] No se pudo leer la llave privada: %s\n", ruta_key);
        return -1;
    }

    char cert_b64[BUFFER_MAX];
    if (cargar_cert_base64(ruta_cert, cert_b64, sizeof(cert_b64)) != 0) {
        fprintf(stderr, "[ERROR] No se pudo leer el certificado: %s\n", ruta_cert);
        EVP_PKEY_free(pkey);
        return -1;
    }

    // 2. Conectar al repositorio (timeout 10s)
    printf("[1] Conectando a %s:%d...\n", host, puerto);
    int sock = conectar_con_timeout(host, puerto);
    if (sock < 0) {
        fprintf(stderr, "[ERROR] No se pudo conectar al repositorio (timeout o puerto cerrado).\n");
        EVP_PKEY_free(pkey);
        return -1;
    }

    // 3. Saludo inicial HELLO
    char linea_envio[BUFFER_MAX];
    snprintf(linea_envio, sizeof(linea_envio), "HELLO|OWNER|%d|%s\n", num_emisor, cert_b64);
    send(sock, linea_envio, strlen(linea_envio), 0);

    // 4. Leer respuesta con el nonce
    char respuesta[BUFFER_MAX];
    if (leer_linea(sock, respuesta, sizeof(respuesta)) <= 0) {
        fprintf(stderr, "[ERROR] Error esperando respuesta del repositorio.\n");
        close(sock);
        EVP_PKEY_free(pkey);
        return -1;
    }
    printf("[2] Respuesta del repositorio: %s\n", respuesta);

    if (strncmp(respuesta, "HELLO_OK|OWNER|", 15) != 0) {
        fprintf(stderr, "[ERROR] Saludo rechazado: %s\n", respuesta);
        close(sock);
        EVP_PKEY_free(pkey);
        return -1;
    }
    const char *nonce = respuesta + 15;
    printf("    Nonce recibido: %s\n", nonce);

    // 5. Construir y firmar el ROA
    time_t ahora = time(NULL);
    long emision = (long)ahora - 10;
    long vence = (long)ahora + duracion_seg;

    char roa_plano[512];
    snprintf(roa_plano, sizeof(roa_plano), "ROA|%s|%d|%d|%ld|%ld", ip_anunciada, isla, num_emisor, emision, vence);

    char firma_roa[1024];
    if (firmar_texto(pkey, roa_plano, firma_roa, sizeof(firma_roa)) != 0) {
        fprintf(stderr, "[ERROR] Error firmando el ROA.\n");
        close(sock);
        EVP_PKEY_free(pkey);
        return -1;
    }

    char roa_completo[1536];
    snprintf(roa_completo, sizeof(roa_completo), "%s|%s", roa_plano, firma_roa);
    printf("[3] ROA firmado: %s\n", roa_completo);

    // 6. Firmar la solicitud PUT contra el nonce y enviarla
    char solicitud_firmar[2048];
    snprintf(solicitud_firmar, sizeof(solicitud_firmar), "PUT|%s|%s", roa_completo, nonce);

    char firma_put[1024];
    if (firmar_texto(pkey, solicitud_firmar, firma_put, sizeof(firma_put)) != 0) {
        fprintf(stderr, "[ERROR] Error firmando orden PUT.\n");
        close(sock);
        EVP_PKEY_free(pkey);
        return -1;
    }

    snprintf(linea_envio, sizeof(linea_envio), "PUT|%s|%s\n", roa_completo, firma_put);
    send(sock, linea_envio, strlen(linea_envio), 0);
    printf("[4] Comando PUT enviado al repositorio.\n");

    // 7. Leer resultado
    if (leer_linea(sock, respuesta, sizeof(respuesta)) > 0) {
        printf("[5] Resultado: %s\n", respuesta);
    }

    // 8. Cerrar conexion
    close(sock);
    EVP_PKEY_free(pkey);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("=== NODO EMISOR (Host) - Publicador de ROAs ===\n\n");
    if (argc < 7) {
        printf("Uso: %s <host_repo> <puerto> <num_emisor> <isla> <ip_anunciada> <ruta_key> <ruta_cert> [duracion_seg]\n", argv[0]);
        printf("Ejemplo:\n");
        printf("  %s 127.0.0.1 9000 101 1 10.0.1.1 ../certificados/nodo.key ../certificados/nodo.crt 86400\n\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int puerto = atoi(argv[2]);
    int num_emisor = atoi(argv[3]);
    int isla = atoi(argv[4]);
    const char *ip = argv[5];
    const char *key = argv[6];
    const char *crt = argv[7];
    long duracion = (argc > 8) ? atol(argv[8]) : 86400;

    return publicar_roa(host, puerto, num_emisor, isla, ip, key, crt, duracion);
}
