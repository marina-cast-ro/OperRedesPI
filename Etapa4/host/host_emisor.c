#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#define TIMEOUT_SEGUNDOS        10
#define BUFFER_MAX              8192
#define INTERVALO_REPUBLICACION 60
#define UMBRAL_RENOVACION_SEG   300

static volatile sig_atomic_t g_ejecutando = 1;

static void manejar_senal(int sig) {
    (void)sig;
    g_ejecutando = 0;
}

static void instalar_manejadores_senal(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejar_senal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

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

// Obtiene la fecha de expiracion (notAfter) de un certificado en segundos Unix UTC
static time_t obtener_not_after_cert(X509 *cert) {
    ASN1_TIME *epoch = ASN1_TIME_set(NULL, 0);
    if (!epoch) return (time_t)-1;
    int days = 0;
    int secs = 0;
    int ok = ASN1_TIME_diff(&days, &secs, epoch, X509_get0_notAfter(cert));
    ASN1_TIME_free(epoch);
    if (!ok) return (time_t)-1;
    return (time_t)days * 86400 + secs;
}

// Carga el archivo .crt, extrae el DER, opcionalmente su notAfter, y lo pasa a Base64
static int cargar_cert_base64(const char *ruta_cert, char *b64_out, size_t max_len, time_t *not_after_out) {
    FILE *f = fopen(ruta_cert, "r");
    if (!f) return -1;

    X509 *cert = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    if (!cert) return -1;

    if (not_after_out != NULL) {
        *not_after_out = obtener_not_after_cert(cert);
    }

    int der_len = i2d_X509(cert, NULL);
    if (der_len <= 0) {
        X509_free(cert);
        return -1;
    }
    unsigned char *der = malloc((size_t)der_len);
    if (!der) {
        X509_free(cert);
        return -1;
    }
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
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

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

// Envia todo el buffer por el socket TCP
static int enviar_todo(int sock, const char *datos, size_t len) {
    size_t enviado = 0;
    while (enviado < len) {
        ssize_t n = send(sock, datos + enviado, len - enviado, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        enviado += (size_t)n;
    }
    return 0;
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

// Extrae el nonce de HELLO_OK|OWNER|<nonce> tolerando espacio o '|' como separador
static int extraer_nonce_hello_ok(const char *resp, const char **nonce_out) {
    if (!resp || !nonce_out) return -1;
    if (strncmp(resp, "HELLO_OK", 8) != 0) return -1;
    const char *p = resp + 8;
    if (*p != '|' && *p != ' ') return -1;
    p++;
    while (*p == ' ') p++;
    if (strncmp(p, "OWNER", 5) != 0) return -1;
    p += 5;
    if (*p != '|' && *p != ' ') return -1;
    p++;
    while (*p == ' ') p++;
    if (*p == '\0') return -1;
    *nonce_out = p;
    return 0;
}

// Acepta OK|<version>, OK <version>, OK o PUT_OK
static int es_respuesta_ok(const char *resp) {
    if (!resp) return 0;
    if (strcmp(resp, "OK") == 0 || strcmp(resp, "PUT_OK") == 0) return 1;
    if (strncmp(resp, "OK", 2) == 0 && (resp[2] == '|' || resp[2] == ' ')) return 1;
    return 0;
}

// Construye y firma una linea ROA|IP|ISLA|NUM_EMISOR|EMISION|VENCE|FIRMA acotando VENCE al notAfter
static int construir_roa_firmado(EVP_PKEY *pkey, const char *ip_anunciada, int isla,
                                 int num_emisor, long duracion_seg, time_t not_after,
                                 char *roa_completo_out, size_t max_roa,
                                 long *emision_out, long *vence_out) {
    time_t ahora = time(NULL);
    long emision = (long)ahora - 10;
    long vence = (long)ahora + duracion_seg;

    if (not_after != (time_t)-1 && vence > (long)not_after) {
        printf("[AVISO] VENCE (%ld) supera notAfter del certificado (%ld); acotando a %ld.\n",
               vence, (long)not_after, (long)not_after);
        vence = (long)not_after;
    }
    if (vence <= (long)ahora || vence <= emision) {
        fprintf(stderr, "[ERROR] El certificado ya vencio o duracion invalida (VENCE=%ld, ahora=%ld).\n",
                vence, (long)ahora);
        return -1;
    }

    char roa_plano[512];
    snprintf(roa_plano, sizeof(roa_plano), "ROA|%s|%d|%d|%ld|%ld",
             ip_anunciada, isla, num_emisor, emision, vence);

    char firma_roa[1024];
    if (firmar_texto(pkey, roa_plano, firma_roa, sizeof(firma_roa)) != 0) {
        fprintf(stderr, "[ERROR] Error firmando el ROA.\n");
        return -1;
    }

    int len = snprintf(roa_completo_out, max_roa, "%s|%s", roa_plano, firma_roa);
    if (len < 0 || (size_t)len >= max_roa) {
        fprintf(stderr, "[ERROR] El ROA firmado excede el tamano del buffer.\n");
        return -1;
    }

    if (emision_out) *emision_out = emision;
    if (vence_out) *vence_out = vence;
    return 0;
}

// Conecta al repositorio, hace HELLO|OWNER, firma PUT contra el nonce y envia el ROA
static int enviar_roa_al_repo(const char *host, int puerto, int num_emisor,
                              EVP_PKEY *pkey, const char *cert_b64, const char *roa_completo) {
    printf("[1] Conectando a %s:%d...\n", host, puerto);
    int sock = conectar_con_timeout(host, puerto);
    if (sock < 0) {
        fprintf(stderr, "[ERROR] No se pudo conectar al repositorio (timeout o puerto cerrado).\n");
        return -1;
    }

    // Saludo inicial HELLO
    char linea_envio[BUFFER_MAX];
    int n_env = snprintf(linea_envio, sizeof(linea_envio), "HELLO|OWNER|%d|%s\n", num_emisor, cert_b64);
    if (n_env < 0 || (size_t)n_env >= sizeof(linea_envio) ||
        enviar_todo(sock, linea_envio, (size_t)n_env) != 0) {
        fprintf(stderr, "[ERROR] Error enviando HELLO al repositorio.\n");
        close(sock);
        return -1;
    }

    // Leer respuesta con el nonce
    char respuesta[4096];
    if (leer_linea(sock, respuesta, sizeof(respuesta)) <= 0) {
        fprintf(stderr, "[ERROR] Error esperando respuesta del repositorio.\n");
        close(sock);
        return -1;
    }
    printf("[2] Respuesta del repositorio: %s\n", respuesta);

    const char *nonce = NULL;
    if (extraer_nonce_hello_ok(respuesta, &nonce) != 0) {
        fprintf(stderr, "[ERROR] Saludo rechazado: %s\n", respuesta);
        close(sock);
        return -1;
    }
    printf("    Nonce recibido: %s\n", nonce);

    // Firmar la solicitud PUT contra el nonce y enviarla
    char solicitud_firmar[BUFFER_MAX];
    int n_sig = snprintf(solicitud_firmar, sizeof(solicitud_firmar), "PUT|%s|%s", roa_completo, nonce);
    if (n_sig < 0 || (size_t)n_sig >= sizeof(solicitud_firmar)) {
        fprintf(stderr, "[ERROR] Solicitud PUT demasiado larga.\n");
        close(sock);
        return -1;
    }

    char firma_put[1024];
    if (firmar_texto(pkey, solicitud_firmar, firma_put, sizeof(firma_put)) != 0) {
        fprintf(stderr, "[ERROR] Error firmando orden PUT.\n");
        close(sock);
        return -1;
    }

    n_env = snprintf(linea_envio, sizeof(linea_envio), "PUT|%s|%s\n", roa_completo, firma_put);
    if (n_env < 0 || (size_t)n_env >= sizeof(linea_envio) ||
        enviar_todo(sock, linea_envio, (size_t)n_env) != 0) {
        fprintf(stderr, "[ERROR] Error enviando orden PUT al repositorio.\n");
        close(sock);
        return -1;
    }
    printf("[4] Comando PUT enviado al repositorio.\n");

    // Leer resultado
    if (leer_linea(sock, respuesta, sizeof(respuesta)) <= 0) {
        fprintf(stderr, "[ERROR] No se recibio respuesta tras PUT.\n");
        close(sock);
        return -1;
    }
    printf("[5] Resultado: %s\n", respuesta);

    close(sock);
    if (!es_respuesta_ok(respuesta)) {
        fprintf(stderr, "[ERROR] Repositorio rechazo PUT: %s\n", respuesta);
        return -1;
    }
    return 0;
}

// Flujo completo para publicar un ROA hacia el repositorio (unico o en bucle cada 60s)
int publicar_roa(const char *host, int puerto, int num_emisor, int isla, const char *ip_anunciada,
                 const char *ruta_key, const char *ruta_cert, long duracion_seg, int modo_bucle) {
    EVP_PKEY *pkey = cargar_llave_privada(ruta_key);
    if (!pkey) {
        fprintf(stderr, "[ERROR] No se pudo leer la llave privada: %s\n", ruta_key);
        return -1;
    }

    char cert_b64[4096];
    time_t not_after = (time_t)-1;
    if (cargar_cert_base64(ruta_cert, cert_b64, sizeof(cert_b64), &not_after) != 0) {
        fprintf(stderr, "[ERROR] No se pudo leer el certificado: %s\n", ruta_cert);
        EVP_PKEY_free(pkey);
        return -1;
    }

    char roa_completo[2048] = "";
    long emision = 0;
    long vence = 0;

    if (!modo_bucle) {
        if (construir_roa_firmado(pkey, ip_anunciada, isla, num_emisor, duracion_seg, not_after,
                                  roa_completo, sizeof(roa_completo), &emision, &vence) != 0) {
            EVP_PKEY_free(pkey);
            return -1;
        }
        printf("[3] ROA firmado: %s\n", roa_completo);
        int rc = enviar_roa_al_repo(host, puerto, num_emisor, pkey, cert_b64, roa_completo);
        EVP_PKEY_free(pkey);
        return rc;
    }

    instalar_manejadores_senal();
    printf("[MODO BUCLE] Republicacion cada %d s; renovacion si faltan < %d s para VENCE. Ctrl+C para salir.\n",
           INTERVALO_REPUBLICACION, UMBRAL_RENOVACION_SEG);

    int tiene_roa = 0;
    while (g_ejecutando) {
        long ahora = (long)time(NULL);
        if (!tiene_roa || (vence - ahora) < UMBRAL_RENOVACION_SEG) {
            if (tiene_roa) {
                printf("\n[RENOVACION] Faltan %ld s para VENCE (< %d s). Firmando nuevo ROA...\n",
                       vence - ahora, UMBRAL_RENOVACION_SEG);
            }
            if (construir_roa_firmado(pkey, ip_anunciada, isla, num_emisor, duracion_seg, not_after,
                                      roa_completo, sizeof(roa_completo), &emision, &vence) != 0) {
                fprintf(stderr, "[ERROR] No se puede emitir ROA valido; deteniendo bucle.\n");
                break;
            } else {
                tiene_roa = 1;
                printf("[3] ROA firmado: %s\n", roa_completo);
            }
        } else {
            printf("\n[REPUBLICACION] Reenviando exactamente el mismo ROA firmado (restan %ld s para VENCE)...\n",
                   vence - ahora);
        }

        if (tiene_roa) {
            enviar_roa_al_repo(host, puerto, num_emisor, pkey, cert_b64, roa_completo);
        }

        for (int s = 0; s < INTERVALO_REPUBLICACION && g_ejecutando; s++) {
            sleep(1);
        }
    }

    printf("\n[MODO BUCLE] Emisor detenido limpiamente.\n");
    EVP_PKEY_free(pkey);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("=== NODO EMISOR (Host) - Publicador de ROAs ===\n\n");

    int modo_bucle = 0;
    const char *pos_args[16];
    int pos_argc = 0;
    pos_args[pos_argc++] = argv[0];

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--loop") == 0 || strcmp(argv[i], "-l") == 0) {
            modo_bucle = 1;
        } else if (pos_argc < 16) {
            pos_args[pos_argc++] = argv[i];
        }
    }

    if (pos_argc < 8) {
        printf("Uso: %s <host_repo> <puerto> <num_emisor> <isla> <ip_anunciada> <ruta_key> <ruta_cert> [duracion_seg] [--loop|-l]\n", argv[0]);
        printf("Ejemplo:\n");
        printf("  %s 172.16.20.10 9000 201 2 172.16.20.10 ../certificados/nodo.key ../certificados/nodo.crt 86400 --loop\n\n", argv[0]);
        return 1;
    }

    const char *host = pos_args[1];
    int puerto = atoi(pos_args[2]);
    int num_emisor = atoi(pos_args[3]);
    int isla = atoi(pos_args[4]);
    const char *ip = pos_args[5];
    const char *key = pos_args[6];
    const char *crt = pos_args[7];
    long duracion = (pos_argc > 8) ? atol(pos_args[8]) : 86400;

    if (puerto <= 0 || puerto > 65535 || duracion <= 0) {
        fprintf(stderr, "[ERROR] Puerto o duracion invalidos.\n");
        return 1;
    }

    return publicar_roa(host, puerto, num_emisor, isla, ip, key, crt, duracion, modo_bucle) == 0 ? 0 : 1;
}
