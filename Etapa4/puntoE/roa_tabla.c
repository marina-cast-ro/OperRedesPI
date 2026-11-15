#include "roa_tabla.h"
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

/* ---------- Utilidades para armar tablas (ya hechas) ---------- */

static int normalizar_isla(int asn) {
    return (asn >= 65001 && asn <= 65031) ? (asn - 65000) : asn;
}

static int ip_coincide_roa(const char *ip_roa, const char *ip_consulta) {
    if (strcmp(ip_roa, ip_consulta) == 0) return 1;
    if (strncmp(ip_consulta, "172.16.", 7) == 0) {
        const char *dot3 = strchr(ip_consulta + 7, '.');
        if (dot3) {
            size_t pref_len = (size_t)(dot3 - ip_consulta) + 1;
            if (strncmp(ip_roa, ip_consulta, pref_len) == 0) return 1;
        }
    }
    return 0;
}

roa_tabla_t *roa_tabla_crear(size_t cap) {
    roa_tabla_t *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->items = calloc(cap ? cap : 1, sizeof *t->items);
    if (!t->items) { free(t); return NULL; }
    t->cap = cap ? cap : 1;
    return t;
}

int roa_tabla_agregar(roa_tabla_t *t, const char *ip, int isla, time_t vence) {
    if (!t || strlen(ip) >= sizeof t->items[0].ip) return -1;
    if (t->n == t->cap) return -1;               /* el llamador define la capacidad */
    strcpy(t->items[t->n].ip, ip);
    t->items[t->n].isla  = normalizar_isla(isla);
    t->items[t->n].vence = vence;
    t->n++;
    return 0;
}

void roa_tabla_destruir(roa_tabla_t *t) {
    if (!t) return;
    free(t->items);
    free(t);
}

/* ---------- Almacen compartido entre los dos hilos ----------
 *
 * Patron lectores-escritores con semaforos POSIX:
 *   sem_lect   : mutex que protege el contador 'lectores'
 *   sem_escr   : "cuarto" de escritura; lo toma el escritor, o el primer lector
 *                (en nombre de todos los lectores) y lo suelta el ultimo lector.
 *
 * Varios lectores entran a la vez; el escritor entra solo.
 * El escritor mantiene el candado solo durante el intercambio de punteros:
 * la tabla nueva ya viene armada y validada desde fuera de la seccion critica. */

static roa_tabla_t *actual = NULL;   /* tabla vigente */
static long version_local = -1;      /* version del repositorio que refleja 'actual' */

static sem_t sem_lect;
static sem_t sem_escr;
static int   lectores = 0;

static void entrar_lectura(void) {
    sem_wait(&sem_lect);
    if (++lectores == 1) sem_wait(&sem_escr);   /* el primer lector bloquea a los escritores */
    sem_post(&sem_lect);
}

static void salir_lectura(void) {
    sem_wait(&sem_lect);
    if (--lectores == 0) sem_post(&sem_escr);   /* el ultimo lector los libera */
    sem_post(&sem_lect);
}

void roa_almacen_iniciar(void) {
    sem_init(&sem_lect, 0, 1);
    sem_init(&sem_escr, 0, 1);
    lectores = 0;
    actual = NULL;
    version_local = -1;
}

void roa_almacen_cerrar(void) {
    roa_tabla_destruir(actual);
    actual = NULL;
    sem_destroy(&sem_lect);
    sem_destroy(&sem_escr);
}

void roa_almacen_publicar(roa_tabla_t *nueva, long version) {
    sem_wait(&sem_escr);            /* acceso exclusivo: ningun lector dentro */
    roa_tabla_t *vieja = actual;
    actual = nueva;                 /* intercambio de puntero (double buffering) */
    version_local = version;
    sem_post(&sem_escr);
    /* Seguro liberar aqui: los lectores solo usan 'actual' dentro del candado,
     * y el que entre ahora ya ve la tabla nueva. Nadie mas tiene la vieja. */
    roa_tabla_destruir(vieja);
}

long roa_almacen_version(void) {
    entrar_lectura();
    long v = version_local;
    salir_lectura();
    return v;
}

roa_resultado_t roa_almacen_consultar(const char *ip, int isla, time_t ahora) {
    roa_resultado_t res = ROA_UNKNOWN;
    int isla_norm = normalizar_isla(isla);
    entrar_lectura();
    if (actual && ip) {
        for (size_t i = 0; i < actual->n; i++) {
            const roa_entrada_t *e = &actual->items[i];
            if (!ip_coincide_roa(e->ip, ip)) continue;
            if (e->vence < ahora) continue;                /* vencido: se trata como inexistente */
            if (normalizar_isla(e->isla) == isla_norm) {
                res = ROA_VALID;
                break;
            }
            res = ROA_INVALID;
        }
    }
    salir_lectura();                /* el resultado se calculo dentro; no se devuelve ningun puntero */
    return res;
}
