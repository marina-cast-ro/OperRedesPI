#ifndef ROA_TABLA_H
#define ROA_TABLA_H

#include <stddef.h>
#include <time.h>

typedef enum { ROA_VALID, ROA_INVALID, ROA_UNKNOWN } roa_resultado_t;

/* Una fila de la tabla local: ya validada criptograficamente (etapa 2). */
typedef struct {
    char ip[16];   /* "255.255.255.255" + '\0' */
    int isla;     /* AS autorizado */
    time_t vence;    /* fin de validez (segundos Unix UTC) */
} roa_entrada_t;

/* Una tabla completa. Se arma FUERA del candado y luego se publica. */
typedef struct {
    roa_entrada_t *items;
    size_t n;
    size_t cap;
} roa_tabla_t;

/* --- Utilidades para armar tablas (ya hechas) --- */
roa_tabla_t *roa_tabla_crear(size_t cap);
int  roa_tabla_agregar(roa_tabla_t *t, const char *ip, int isla, time_t vence);
void roa_tabla_destruir(roa_tabla_t *t);

/* --- Almacen compartido entre los dos hilos (TU PARTE) --- */
void roa_almacen_iniciar(void);
void roa_almacen_cerrar(void);
/* Hilo actualizador: instala 'nueva' como tabla vigente. Toma posesion de 'nueva'. */
void roa_almacen_publicar(roa_tabla_t *nueva, long version);
long roa_almacen_version(void);
/* Hilo de enrutamiento: evalua un anuncio. 'ahora' se pasa para poder probarlo. */
roa_resultado_t roa_almacen_consultar(const char *ip, int isla, time_t ahora);

#endif
