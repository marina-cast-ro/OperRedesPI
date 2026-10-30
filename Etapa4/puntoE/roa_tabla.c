#include "roa_tabla.h"
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

/* ---------- Utilidades para armar tablas (ya hechas) ---------- */

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
    t->items[t->n].isla  = isla;
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
    entrar_lectura();
    if (actual) {
        for (size_t i = 0; i < actual->n; i++) {
            const roa_entrada_t *e = &actual->items[i];
            if (strcmp(e->ip, ip) != 0) continue;
            if (e->vence < ahora)      res = ROA_UNKNOWN;   /* vencido: se trata como inexistente */
            else if (e->isla == isla)  res = ROA_VALID;
            else                       res = ROA_INVALID;
            break;
        }
    }
    salir_lectura();                /* el resultado se calculo dentro; no se devuelve ningun puntero */
    return res;
}
