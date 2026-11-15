#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PUERTO_DEFECTO   5005
#define ARCHIVO_DEFECTO  "mensajes_recibidos.txt"
#define MAX_LINEA        2048

static volatile sig_atomic_t g_ejecutando = 1;
static int g_sock_servidor = -1;

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

static int es_entero_positivo(const char *s) {
    if (!s || *s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

static void registrar_mensaje(const char *archivo_salida, const char *ip_cliente, const char *linea) {
    if (!linea || linea[0] == '\0') return;

    char hora[32];
    time_t ahora = time(NULL);
    struct tm tm_info;
    localtime_r(&ahora, &tm_info);
    strftime(hora, sizeof(hora), "%Y-%m-%d %H:%M:%S", &tm_info);

    int origen = 0, destino = 0, ttl = 0;
    char datos[MAX_LINEA];
    int tiene_origen = 0;

    if (sscanf(linea, "DATA|%d|%d|%d|%2047[^\n]", &origen, &destino, &ttl, datos) == 4) {
        tiene_origen = 1;
    } else {
        const char *pipe = strchr(linea, '|');
        if (pipe && pipe > linea) {
            size_t pref_len = (size_t)(pipe - linea);
            if (pref_len < 16) {
                char prefijo[16];
                memcpy(prefijo, linea, pref_len);
                prefijo[pref_len] = '\0';
                if (es_entero_positivo(prefijo)) {
                    origen = atoi(prefijo);
                    tiene_origen = 1;
                    snprintf(datos, sizeof(datos), "%s", pipe + 1);
                }
            }
        }
        if (!tiene_origen) {
            snprintf(datos, sizeof(datos), "%s", linea);
        }
    }

    if (tiene_origen) {
        printf("[%s] [RECEPTOR] cliente=%s isla_origen=%d datos=\"%s\"\n",
               hora, ip_cliente, origen, datos);
    } else {
        printf("[%s] [RECEPTOR] cliente=%s isla_origen=N/A datos=\"%s\"\n",
               hora, ip_cliente, datos);
    }
    fflush(stdout);

    FILE *f = fopen(archivo_salida, "a");
    if (!f) {
        perror("[RECEPTOR] No se pudo abrir el archivo de salida");
        return;
    }
    if (tiene_origen) {
        fprintf(f, "[%s] [RECEPTOR] cliente=%s isla_origen=%d datos=\"%s\"\n",
                hora, ip_cliente, origen, datos);
    } else {
        fprintf(f, "[%s] [RECEPTOR] cliente=%s isla_origen=N/A datos=\"%s\"\n",
                hora, ip_cliente, datos);
    }
    fclose(f);
}

static void atender_cliente(int client_sock, const char *ip_cliente, const char *archivo_salida) {
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char linea[MAX_LINEA];
    size_t idx = 0;

    while (1) {
        char c;
        ssize_t n = recv(client_sock, &c, 1, 0);
        if (n <= 0) {
            if (idx > 0) {
                linea[idx] = '\0';
                registrar_mensaje(archivo_salida, ip_cliente, linea);
            }
            break;
        }
        if (c == '\r') continue;
        if (c == '\n') {
            linea[idx] = '\0';
            if (idx > 0) {
                registrar_mensaje(archivo_salida, ip_cliente, linea);
            }
            idx = 0;
            continue;
        }
        if (idx < sizeof(linea) - 1) {
            linea[idx++] = c;
        }
    }
}

int main(int argc, char *argv[]) {
    int puerto = (argc >= 2) ? atoi(argv[1]) : PUERTO_DEFECTO;
    const char *archivo_salida = (argc >= 3) ? argv[2] : ARCHIVO_DEFECTO;

    if (puerto <= 0 || puerto > 65535) {
        fprintf(stderr, "Uso: %s [puerto_tcp] [archivo_salida]\n", argv[0]);
        return 1;
    }

    instalar_manejadores_senal();

    g_sock_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock_servidor < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(g_sock_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((uint16_t)puerto);

    if (bind(g_sock_servidor, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(g_sock_servidor);
        return 1;
    }

    if (listen(g_sock_servidor, 16) < 0) {
        perror("listen");
        close(g_sock_servidor);
        return 1;
    }

    printf("=== HOST RECEPTOR ===\n");
    printf("Escuchando en TCP puerto %d, guardando en '%s'...\n", puerto, archivo_salida);
    fflush(stdout);

    while (g_ejecutando) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(g_sock_servidor, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR && !g_ejecutando) break;
            if (!g_ejecutando) break;
            continue;
        }

        char ip_cliente[INET_ADDRSTRLEN] = "desconocida";
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_cliente, sizeof(ip_cliente));

        atender_cliente(client_sock, ip_cliente, archivo_salida);
        close(client_sock);
    }

    if (g_sock_servidor >= 0) {
        close(g_sock_servidor);
        g_sock_servidor = -1;
    }
    printf("\nReceptor detenido limpiamente.\n");
    return 0;
}
