#include "repo_cliente.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINEA 16384          /* un certificado en base64 ocupa unos 2 KB; sobra margen */

typedef struct {
    int    fd;
    int    timeout_ms;
    char   buf[MAX_LINEA * 2];   /* bytes recibidos aun sin consumir */
    size_t largo;
} sesion_t;

static void poner_err(char *err, size_t n, const char *msg) {
    if (err && n) snprintf(err, n, "%s", msg);
}

static long ms_ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/* Conecta con timeout (socket no bloqueante + poll). Devuelve fd o -1. */
static int conectar(const repo_cfg_t *cfg, char *err, size_t errlen) {
    char puerto[16];
    snprintf(puerto, sizeof puerto, "%d", cfg->puerto);
    struct addrinfo pistas, *res = NULL;
    memset(&pistas, 0, sizeof pistas);
    pistas.ai_family = AF_INET;
    pistas.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(cfg->host, puerto, &pistas, &res) != 0 || !res) {
        poner_err(err, errlen, "no se pudo resolver el repositorio");
        return -1;
    }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); poner_err(err, errlen, "socket() fallo"); return -1; }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    int r = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (r < 0 && errno != EINPROGRESS) {
        close(fd);
        poner_err(err, errlen, "connect() fallo");
        return -1;
    }
    if (r < 0) {
        struct pollfd p = { .fd = fd, .events = POLLOUT };
        int pr;
        do { pr = poll(&p, 1, cfg->timeout_s * 1000); } while (pr < 0 && errno == EINTR);
        int soerr = 0;
        socklen_t sl = sizeof soerr;
        if (pr <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0 || soerr != 0) {
            close(fd);
            poner_err(err, errlen, pr == 0 ? "timeout al conectar" : "no se pudo conectar");
            return -1;
        }
    }
    return fd;
}

static int enviar_todo(sesion_t *s, const char *datos, size_t n) {
    size_t env = 0;
    long fin = ms_ahora() + s->timeout_ms;
    while (env < n) {
        struct pollfd p = { .fd = s->fd, .events = POLLOUT };
        long resta = fin - ms_ahora();
        if (resta <= 0) return -1;
        int pr = poll(&p, 1, (int)resta);
        if (pr < 0 && errno == EINTR) continue;
        if (pr <= 0) return -1;
        ssize_t k = send(s->fd, datos + env, n - env, MSG_NOSIGNAL);
        if (k < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (k <= 0) return -1;
        env += (size_t)k;
    }
    return 0;
}

static int enviar_linea(sesion_t *s, const char *linea) {
    char *msg = malloc(strlen(linea) + 2);
    if (!msg) return -1;
    sprintf(msg, "%s\n", linea);
    int r = enviar_todo(s, msg, strlen(msg));
    free(msg);
    return r;
}

/* Lee una linea (sin el '\n'), esperando como maximo timeout_ms por cada llamada.
 * Devuelve una cadena malloc o NULL (timeout, cierre, linea demasiado larga). */
static char *leer_linea(sesion_t *s) {
    long fin = ms_ahora() + s->timeout_ms;
    while (1) {
        char *nl = memchr(s->buf, '\n', s->largo);
        if (nl) {
            size_t l = (size_t)(nl - s->buf);
            char *out = malloc(l + 1);
            if (!out) return NULL;
            memcpy(out, s->buf, l);
            out[l] = '\0';
            if (l && out[l - 1] == '\r') out[l - 1] = '\0';
            memmove(s->buf, nl + 1, s->largo - l - 1);
            s->largo -= l + 1;
            return out;
        }
        if (s->largo >= MAX_LINEA) return NULL;
        long resta = fin - ms_ahora();
        if (resta <= 0) return NULL;
        struct pollfd p = { .fd = s->fd, .events = POLLIN };
        int pr = poll(&p, 1, (int)resta);
        if (pr < 0 && errno == EINTR) continue;
        if (pr <= 0) return NULL;
        ssize_t k = recv(s->fd, s->buf + s->largo, sizeof s->buf - s->largo, 0);
        if (k < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (k <= 0) return NULL;       /* cerro antes de terminar la linea */
        s->largo += (size_t)k;
    }
}

/* Abre la conexion y hace el HELLO del enrutador. */
static repo_estado_t abrir(const repo_cfg_t *cfg, sesion_t *s, char *err, size_t errlen) {
    s->largo = 0;
    s->timeout_ms = cfg->timeout_s * 1000;
    s->fd = conectar(cfg, err, errlen);
    if (s->fd < 0) return REPO_ERR_RED;

    char hello[128];
    snprintf(hello, sizeof hello, "HELLO|ENRUTADOR|%s", cfg->id);
    if (enviar_linea(s, hello) < 0) {
        close(s->fd); poner_err(err, errlen, "no se pudo enviar el HELLO"); return REPO_ERR_RED;
    }
    char *resp = leer_linea(s);
    if (!resp) {
        close(s->fd); poner_err(err, errlen, "sin respuesta al HELLO"); return REPO_ERR_RED;
    }
    int ok = strcmp(resp, "HELLO_OK|ENRUTADOR|-") == 0;
    if (!ok) poner_err(err, errlen, strncmp(resp, "ERR_", 4) == 0 ? resp : "respuesta inesperada al HELLO");
    free(resp);
    if (!ok) { close(s->fd); return REPO_ERR_PROTO; }    /* HELLO rechazado: se aborta */
    return REPO_OK;
}

void repo_liberar_lineas(char **lineas, size_t n) {
    if (!lineas) return;
    for (size_t i = 0; i < n; i++) free(lineas[i]);
    free(lineas);
}

repo_estado_t repo_list(const repo_cfg_t *cfg, long *version, char ***lineas, size_t *n, char *err, size_t errlen) {
    sesion_t *s = malloc(sizeof *s);
    if (!s) { poner_err(err, errlen, "sin memoria"); return REPO_ERR_RED; }
    repo_estado_t est = abrir(cfg, s, err, errlen);
    if (est != REPO_OK) { free(s); return est; }

    char **v = NULL;
    size_t cant = 0, leidas = 0;
    char *cab = NULL;

    if (enviar_linea(s, "LIST") < 0 || !(cab = leer_linea(s))) {
        poner_err(err, errlen, "sin respuesta al LIST");
        est = REPO_ERR_RED;
        goto fin;
    }
    if (strncmp(cab, "ERR_", 4) == 0) {
        poner_err(err, errlen, cab);
        est = REPO_ERR_PROTO;
        goto fin;
    }
    long ver, cnt;
    char extra;
    if (sscanf(cab, "OK|%ld|%ld%c", &ver, &cnt, &extra) != 2 || cnt < 0 || cnt > 100000) {
        poner_err(err, errlen, "cabecera de LIST mal formada");
        est = REPO_ERR_PROTO;
        goto fin;
    }
    cant = (size_t)cnt;
    v = calloc(cant ? cant : 1, sizeof *v);
    if (!v) { poner_err(err, errlen, "sin memoria"); est = REPO_ERR_RED; goto fin; }
    for (; leidas < cant; leidas++) {
        v[leidas] = leer_linea(s);
        if (!v[leidas]) {            /* llegaron menos de n lineas: se descarta TODA la descarga */
            poner_err(err, errlen, "LIST incompleto o con timeout");
            est = REPO_ERR_RED;
            goto fin;
        }
    }
    *version = ver;
    *lineas = v;
    *n = cant;
    v = NULL;
    est = REPO_OK;

fin:
    if (v) repo_liberar_lineas(v, leidas);
    free(cab);
    close(s->fd);
    free(s);
    return est;
}

repo_estado_t repo_cert(const repo_cfg_t *cfg, long num_emisor, char **cert_b64, char *err, size_t errlen) {
    sesion_t *s = malloc(sizeof *s);
    if (!s) { poner_err(err, errlen, "sin memoria"); return REPO_ERR_RED; }
    repo_estado_t est = abrir(cfg, s, err, errlen);
    if (est != REPO_OK) { free(s); return est; }

    char cmd[64];
    snprintf(cmd, sizeof cmd, "CERT|%ld", num_emisor);
    char *resp = NULL;
    if (enviar_linea(s, cmd) < 0 || !(resp = leer_linea(s))) {
        poner_err(err, errlen, "sin respuesta al CERT");
        est = REPO_ERR_RED;
    } else if (strncmp(resp, "OK|", 3) == 0 && resp[3] != '\0') {
        *cert_b64 = strdup(resp + 3);
        est = *cert_b64 ? REPO_OK : REPO_ERR_RED;
    } else {
        poner_err(err, errlen, strncmp(resp, "ERR_", 4) == 0 ? resp : "respuesta inesperada al CERT");
        est = REPO_ERR_PROTO;
    }
    free(resp);
    close(s->fd);
    free(s);
    return est;
}
