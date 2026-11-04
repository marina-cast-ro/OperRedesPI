#ifndef ROA_VALIDATE_H
#define ROA_VALIDATE_H

#include <stddef.h>
#include <time.h>
#include <openssl/x509.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

// ========================= STRUCTS =========================
/** ROA separado en campos bajo el formato:
    ROA|IP|ISLA|NUM_EMISOR|EMISION|VENCE|FIRMA */
typedef struct {
    char ip[16];
    int  isla;
    long num_emisor;
    long emision;
    long vence;
    char *texto_firmado;  // "ROA|IP|ISLA|NUM_EMISOR|EMISION|VENCE" (lo que se firmo)
    unsigned char *firma; // firma binaria (ya decodificada de base64)
    size_t firma_len;
} roa_t;

/* Raices de confianza leidas de trusted-roots.pem (punto A). */
typedef struct {
    X509 **v;
    size_t n;
} roa_trusted_roots_t;

/* Resultado de validar un ROA. Los "recuperables" pueden cambiar sin que cambie la
 * version del repositorio (llego una raiz nueva al truststore, el reloj se ajusto...),
 * asi que el sincronizador no debe dar por buena la version si ve alguno. */
typedef enum {
    ROA_OK = 0,
    ROA_RECHAZADO,               /* definitivo para esta version del repositorio */
    ROA_RECHAZADO_RECUPERABLE
} roa_veredicto_t;

// ========================= FUNCIONES =========================

/* --- Validacion --- */
/* Punto A, 05_validar_cadena: vigencia, emisor en el truststore, firma de la CA y
* extensiones criticas. Devuelve ROA_OK o el veredicto con el motivo en 'motivo'. */
roa_veredicto_t roa_validar_cadena(X509 *cert, const roa_trusted_roots_t *raices, time_t ahora, const char **motivo);

/* Punto B, pasos 2 a 6 de la validacion de un ROA. 'cert' viene de CERT|<num_emisor>. */
roa_veredicto_t roa_validar(const roa_t *roa, X509 *cert, const roa_trusted_roots_t *raices, time_t ahora, const char **motivo);

/* --- Parseo --- */
/* 0 si esta bien; -1 si el formato es incorrecto (escribe el motivo en err). */
int roa_parse(const char *linea, roa_t *out, char *err, size_t errlen);
void roa_free(roa_t *r);

/* ----------------------- Truststore ----------------------- */
// Lee todas las raices del bundle PEM. Devuelve 0, o -1 si no hay ninguna.
int roa_charge_roots(const char *ruta, roa_trusted_roots_t *out, char *err, size_t errlen);
void roa_free_roots(roa_trusted_roots_t *r);

/* --- Certificados --- */
/* Acepta base64(DER) o base64(PEM completo), como en el punto A. NULL si no se puede leer. */
X509 *roa_cert_desde_b64(const char *b64);

/* --- Validacion --- */
/* Punto A, 05_validar_cadena: vigencia, emisor en el truststore, firma de la CA y
* extensiones criticas. Devuelve ROA_OK o el veredicto con el motivo en 'motivo'. */
roa_veredicto_t roa_validar_cadena(X509 *cert, const roa_trusted_roots_t *raices, time_t ahora, const char **motivo);

/* Punto B, pasos 2 a 6 de la validacion de un ROA. 'cert' viene de CERT|<num_emisor>. */
roa_veredicto_t roa_validar(const roa_t *roa, X509 *cert, const roa_trusted_roots_t *raices, time_t ahora, const char **motivo);

#endif
