#define _POSIX_C_SOURCE 200809L
#include "roa_sync.h"
#include "roa_validar.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------------- bitacora ---------------- */

static punto_e_log_fn log_fn = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void punto_e_set_log(punto_e_log_fn fn) { log_fn = fn; }

static void logf_(const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&log_mutex);
    if (log_fn) {
        log_fn(msg);
    } else {
        time_t t = time(NULL);
        struct tm tm;
        char hora[32];
        gmtime_r(&t, &tm);
        strftime(hora, sizeof hora, "%H:%M:%S", &tm);
        fprintf(stderr, "[%sZ] [ROA] %s\n", hora, msg);
    }
    pthread_mutex_unlock(&log_mutex);
}

/* ---------------- configuracion ---------------- */

punto_e_cfg_t punto_e_cfg_defecto(const char *host_repo, const char *id_enrutador) {
    punto_e_cfg_t c;
    memset(&c, 0, sizeof c);
    snprintf(c.repo.host, sizeof c.repo.host, "%s", host_repo);
    c.repo.puerto = 9000;
    c.repo.timeout_s = 10;
    snprintf(c.repo.id, sizeof c.repo.id, "%s", id_enrutador);
    snprintf(c.ruta_truststore, sizeof c.ruta_truststore, "/etc/rpki/truststore/trusted-roots.pem");
    c.periodo_s = 75;
    c.espera_inicial_max_s = 75;
    return c;
}

/* ---------------- cache de certificados dentro de un ciclo ---------------- */

typedef struct {
    long  num;
    X509 *cert;        /* NULL si no hay certificado utilizable */
    char  motivo[96];  /* por que no hay certificado (ERR_NO_EXISTE, ilegible...) */
} cert_cache_t;

typedef struct {
    cert_cache_t *v;
    size_t n, cap;
} cache_t;

static void cache_liberar(cache_t *c) {
    for (size_t i = 0; i < c->n; i++) X509_free(c->v[i].cert);
    free(c->v);
    memset(c, 0, sizeof *c);
}

/* Devuelve la entrada del cache, pidiendo CERT al repositorio si hace falta.
 * NULL = fallo de RED: hay que abandonar el ciclo y conservar la tabla anterior. */
static cert_cache_t *cache_obtener(cache_t *c, const repo_cfg_t *cfg, long num) {
    for (size_t i = 0; i < c->n; i++)
        if (c->v[i].num == num) return &c->v[i];

    char *b64 = NULL;
    char err[128] = "";
    repo_estado_t est = repo_cert(cfg, num, &b64, err, sizeof err);
    if (est == REPO_ERR_RED) {
        logf_("fallo de red pidiendo CERT|%ld: %s", num, err);
        return NULL;
    }
    cert_cache_t nuevo;
    memset(&nuevo, 0, sizeof nuevo);
    nuevo.num = num;
    if (est == REPO_OK) {
        nuevo.cert = roa_cert_desde_b64(b64);
        if (!nuevo.cert) snprintf(nuevo.motivo, sizeof nuevo.motivo, "certificado ilegible");
        free(b64);
    } else if (strcmp(err, "ERR_NO_EXISTE") == 0) {
        snprintf(nuevo.motivo, sizeof nuevo.motivo, "el repositorio no tiene certificado para ese emisor");
    } else {
        /* HELLO rechazado u otro error del protocolo: no es culpa del ROA */
        logf_("error de protocolo pidiendo CERT|%ld: %s", num, err);
        return NULL;
    }
    if (c->n == c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 16;
        cert_cache_t *nv = realloc(c->v, nc * sizeof *nv);
        if (!nv) { X509_free(nuevo.cert); return NULL; }
        c->v = nv;
        c->cap = nc;
    }
    c->v[c->n] = nuevo;
    return &c->v[c->n++];
}

/* ---------------- un ciclo de sincronizacion ---------------- */

ciclo_resultado_t punto_e_ciclo(const punto_e_cfg_t *cfg, time_t ahora) {
    char err[160] = "";
    long ver_remota;
    char **lineas = NULL;
    size_t n = 0;

    /* 1. Descargar. Cualquier fallo -> se conserva la tabla anterior. */
    repo_estado_t est = repo_list(&cfg->repo, &ver_remota, &lineas, &n, err, sizeof err);
    if (est != REPO_OK) {
        logf_("descarga fallida (%s); se conserva la tabla anterior", err);
        return CICLO_FALLO;
    }

    /* 2. Misma version: no se hace procesamiento criptografico. */
    if (ver_remota == roa_almacen_version()) {
        repo_liberar_lineas(lineas, n);
        return CICLO_SIN_CAMBIOS;
    }

    /* 3. Todo el trabajo pesado, FUERA del candado. */
    roa_raices_t raices;
    if (roa_raices_cargar(cfg->ruta_truststore, &raices, err, sizeof err) != 0) {
        logf_("truststore no disponible (%s); se conserva la tabla anterior", err);
        repo_liberar_lineas(lineas, n);
        return CICLO_FALLO;
    }

    roa_tabla_t *nueva = roa_tabla_crear(n ? n : 1);
    cache_t cache;
    memset(&cache, 0, sizeof cache);
    int recuperables = 0;
    ciclo_resultado_t res = CICLO_ACTUALIZADO;

    if (!nueva) { res = CICLO_FALLO; goto limpiar; }

    for (size_t i = 0; i < n; i++) {
        roa_t roa;
        if (roa_parsear(lineas[i], &roa, err, sizeof err) != 0) {
            logf_("ROA descartado: %s", err);
            continue;
        }
        cert_cache_t *cc = cache_obtener(&cache, &cfg->repo, roa.num_emisor);
        if (!cc) {                         /* fallo de red: abandonar el ciclo completo */
            roa_liberar(&roa);
            res = CICLO_FALLO;
            goto limpiar;
        }
        if (!cc->cert) {
            logf_("ROA %s (isla %d, emisor %ld) descartado: %s", roa.ip, roa.isla, roa.num_emisor, cc->motivo);
            roa_liberar(&roa);
            continue;
        }
        const char *motivo = "";
        roa_veredicto_t v = roa_validar(&roa, cc->cert, &raices, ahora, &motivo);
        if (v == ROA_OK) {
            if (roa_tabla_agregar(nueva, roa.ip, roa.isla, (time_t)roa.vence) != 0)
                logf_("ROA %s descartado: IP repetida o tabla llena", roa.ip);
        } else {
            logf_("ROA %s (isla %d, emisor %ld) descartado: %s", roa.ip, roa.isla, roa.num_emisor, motivo);
            if (v == ROA_RECHAZADO_RECUPERABLE) recuperables++;
        }
        roa_liberar(&roa);
    }

    /* 4. Publicacion: solo el intercambio de punteros va dentro del candado.
     *    Si hubo rechazos "recuperables" (raiz que aun no esta en el truststore, reloj...),
     *    se publica la tabla pero con version -1, para que el siguiente ciclo vuelva a
     *    validar aunque el repositorio no haya cambiado. */
    logf_("version %ld: %zu de %zu ROA validos%s", ver_remota, nueva->n, n, recuperables ? " (se reintentara el siguiente ciclo)" : "");
    roa_almacen_publicar(nueva, recuperables ? -1 : ver_remota);
    nueva = NULL;

limpiar:
    roa_tabla_destruir(nueva);
    cache_liberar(&cache);
    roa_raices_liberar(&raices);
    repo_liberar_lineas(lineas, n);
    if (res == CICLO_FALLO) logf_("ciclo abortado; se conserva la tabla anterior");
    return res;
}

/* ---------------- hilo sincronizador ---------------- */

static pthread_t hilo;
static atomic_int hilo_activo = 0;
static atomic_int detener = 0;
static punto_e_cfg_t cfg_hilo;

/* Duerme 'segundos' pero se despierta pronto si piden detener. */
static void dormir_interrumpible(int segundos) {
    for (int ms = 0; ms < segundos * 1000 && !atomic_load(&detener); ms += 100) {
        struct timespec t = { 0, 100 * 1000 * 1000 };
        nanosleep(&t, NULL);
    }
}

static void *bucle_sincronizador(void *arg) {
    (void)arg;
    unsigned int semilla = (unsigned int)time(NULL) ^ (unsigned int)getpid() ^ (unsigned int)(size_t)&semilla;
    int espera = cfg_hilo.espera_inicial_max_s > 0 ? (int)(rand_r(&semilla) % (cfg_hilo.espera_inicial_max_s + 1)) : 0;
    logf_("primera consulta en %d s", espera);
    dormir_interrumpible(espera);

    while (!atomic_load(&detener)) {
        punto_e_ciclo(&cfg_hilo, time(NULL));
        dormir_interrumpible(cfg_hilo.periodo_s);
    }
    return NULL;
}

int punto_e_iniciar(const punto_e_cfg_t *cfg) {
    if (atomic_load(&hilo_activo)) return -1;
    cfg_hilo = *cfg;
    atomic_store(&detener, 0);
    roa_almacen_iniciar();
    if (pthread_create(&hilo, NULL, bucle_sincronizador, NULL) != 0) {
        roa_almacen_cerrar();
        return -1;
    }
    atomic_store(&hilo_activo, 1);
    return 0;
}

void punto_e_detener(void) {
    if (!atomic_load(&hilo_activo)) return;
    atomic_store(&detener, 1);
    pthread_join(hilo, NULL);
    atomic_store(&hilo_activo, 0);
    roa_almacen_cerrar();
}

/* ---------------- consulta ---------------- */

roa_resultado_t punto_e_evaluar(const char *ip, int isla) {
    /* La hora se toma AQUI, en el instante de evaluar, para no aceptar un ROA vencido
     * que siga en la tabla local (que puede tener hasta 75 s de antiguedad). */
    return roa_almacen_consultar(ip, isla, time(NULL));
}
