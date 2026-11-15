#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define IP_ROUTER_DEFECTO     "172.16.20.1"
#define PUERTO_ROUTER_DEFECTO 5005
#define ISLA_ORIGEN_DEFECTO   2
#define TTL_DEFECTO           8
#define TIMEOUT_SEGUNDOS      5

static int conectar_con_timeout(const char *ip, int puerto) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        close(sock);
        return -1;
    }
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

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

    struct timeval tv = { .tv_sec = TIMEOUT_SEGUNDOS, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    return sock;
}

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

static void imprimir_uso(const char *prog) {
    printf("Uso completo:\n");
    printf("  %s <ip_router> <puerto_router> <isla_origen> <isla_destino> <mensaje> [ttl]\n", prog);
    printf("Uso abreviado (usa %s:%d, origen=%d, ttl=%d):\n",
           IP_ROUTER_DEFECTO, PUERTO_ROUTER_DEFECTO, ISLA_ORIGEN_DEFECTO, TTL_DEFECTO);
    printf("  %s <isla_destino> <mensaje>\n", prog);
}

int main(int argc, char *argv[]) {
    const char *ip_router = IP_ROUTER_DEFECTO;
    int puerto_router = PUERTO_ROUTER_DEFECTO;
    int isla_origen = ISLA_ORIGEN_DEFECTO;
    int isla_destino = 0;
    const char *mensaje = NULL;
    int ttl = TTL_DEFECTO;

    if (argc == 3) {
        isla_destino = atoi(argv[1]);
        mensaje = argv[2];
    } else if (argc >= 6 && argc <= 7) {
        ip_router = argv[1];
        puerto_router = atoi(argv[2]);
        isla_origen = atoi(argv[3]);
        isla_destino = atoi(argv[4]);
        mensaje = argv[5];
        if (argc == 7) ttl = atoi(argv[6]);
    } else {
        imprimir_uso(argv[0]);
        return 1;
    }

    if (puerto_router <= 0 || puerto_router > 65535 ||
        isla_origen <= 0 || isla_destino <= 0 || ttl <= 0 ||
        !mensaje || mensaje[0] == '\0') {
        fprintf(stderr, "[ERROR] Argumentos invalidos.\n");
        imprimir_uso(argv[0]);
        return 1;
    }

    char paquete[2048];
    int len = snprintf(paquete, sizeof(paquete), "DATA|%d|%d|%d|%s\n",
                       isla_origen, isla_destino, ttl, mensaje);
    if (len < 0 || (size_t)len >= sizeof(paquete)) {
        fprintf(stderr, "[ERROR] El mensaje es demasiado largo.\n");
        return 1;
    }

    int sock = conectar_con_timeout(ip_router, puerto_router);
    if (sock < 0) {
        fprintf(stderr, "[ERROR] No se pudo conectar al enrutador %s:%d.\n",
                ip_router, puerto_router);
        return 1;
    }

    if (enviar_todo(sock, paquete, (size_t)len) != 0) {
        fprintf(stderr, "[ERROR] Fallo el envio hacia %s:%d.\n",
                ip_router, puerto_router);
        close(sock);
        return 1;
    }

    close(sock);
    printf("[OK] Enviado a %s:%d -> %.*s\n", ip_router, puerto_router, len - 1, paquete);
    return 0;
}
