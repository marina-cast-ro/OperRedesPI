#ifndef ROA_SYNC_H
#define ROA_SYNC_H

#include <time.h>
#include "repo_cliente.h"
#include "roa_tabla.h"

/* Punto E: validacion de ROAs en el enrutador.
 *
 * Uso desde el enrutador:
 *     punto_e_cfg_t cfg = punto_e_cfg_defecto("10.0.0.9", "ENR-3");
 *     punto_e_iniciar(&cfg);                       // arranca el hilo sincronizador
 *     ...
 *     roa_resultado_t r = punto_e_evaluar(ip, isla);   // por cada anuncio recibido
 *     if (r == ROA_INVALID) descartar();
 *     ...
 *     punto_e_detener();
 */

typedef struct {
    repo_cfg_t  repo;
    char        ruta_truststore[256];   /* /etc/rpki/truststore/trusted-roots.pem */
    int         periodo_s;              /* 75 */
    int         espera_inicial_max_s;   /* 75: espera aleatoria 0..N antes de la primera consulta */
} punto_e_cfg_t;

/* Valores acordados: puerto 9000, timeout 10 s, periodo 75 s, espera inicial 0-75 s. */
punto_e_cfg_t punto_e_cfg_defecto(const char *host_repo, const char *id_enrutador);

/* Resultado de un ciclo de sincronizacion. */
typedef enum {
    CICLO_ACTUALIZADO = 0,    /* se publico una tabla nueva */
    CICLO_SIN_CAMBIOS = 1,    /* version remota == version local: no se valido nada */
    CICLO_FALLO = -1          /* red, protocolo o truststore: se conserva la tabla anterior */
} ciclo_resultado_t;

/* Un ciclo completo (lo que hace el hilo cada 'periodo_s'). Expuesto para pruebas.
 * 'ahora' es la hora UTC en segundos Unix. */
ciclo_resultado_t punto_e_ciclo(const punto_e_cfg_t *cfg, time_t ahora);

/* Arranca el almacen y el hilo sincronizador. 0 si esta bien. */
int  punto_e_iniciar(const punto_e_cfg_t *cfg);
/* Detiene el hilo y libera todo. */
void punto_e_detener(void);

/* Consulta del hilo de enrutamiento: usa la tabla local y la hora actual en ese instante. */
roa_resultado_t punto_e_evaluar(const char *ip, int isla);

/* Bitacora: por defecto escribe en stderr. El enrutador puede reemplazarla. */
typedef void (*punto_e_log_fn)(const char *mensaje);
void punto_e_set_log(punto_e_log_fn fn);

#endif
