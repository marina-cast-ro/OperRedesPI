#ifndef REPO_CLIENTE_H
#define REPO_CLIENTE_H

#include <stddef.h>

/* Cliente del repositorio de ROAs (punto C): TCP, una conexion por tramite
 * (HELLO + un comando + cierre), lineas terminadas en '\n', timeout de 10 s. */

typedef struct {
    char host[64];      /* IP o nombre del repositorio */
    int  puerto;        /* 9000 en el protocolo acordado */
    char id[32];        /* identificador del enrutador para HELLO|ENRUTADOR|<id> */
    int  timeout_s;     /* 10 en el protocolo acordado */
} repo_cfg_t;

typedef enum {
    REPO_OK = 0,
    REPO_ERR_RED = -1,   /* no conecta, timeout, cierre a medias, respuesta incompleta */
    REPO_ERR_PROTO = -2  /* el repositorio respondio ERR_xxx o algo que no entendemos */
} repo_estado_t;

/* LIST: version + n lineas ROA. Si es REPO_OK, 'lineas' es un arreglo de n cadenas malloc
 * que se libera con repo_liberar_lineas. */
repo_estado_t repo_list(const repo_cfg_t *cfg, long *version, char ***lineas, size_t *n, char *err, size_t errlen);

/* CERT|<num_emisor>: devuelve el certificado en base64 (malloc). Si el repositorio
 * responde ERR_NO_EXISTE, devuelve REPO_ERR_PROTO y escribe "ERR_NO_EXISTE" en err. */
repo_estado_t repo_cert(const repo_cfg_t *cfg, long num_emisor, char **cert_b64, char *err, size_t errlen);

void repo_liberar_lineas(char **lineas, size_t n);

#endif
