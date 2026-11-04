#include "vector_distancia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Inicializa la tabla con costos en infinito
void tabla_iniciar(tabla_enrutamiento_t *t, int mi_id, int puerto) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->mi_id = mi_id;
    t->puerto_tcp = puerto ? puerto : PUERTO_TCP_DEFECTO;
    pthread_mutex_init(&t->candado, NULL);

    for (int d = 0; d < MAX_ISLAS; d++) {
        for (int v = 0; v < MAX_ISLAS; v++) {
            t->matriz_costos[d][v] = INF;
        }
    }
}

// Destruye el mutex de la tabla
void tabla_destruir(tabla_enrutamiento_t *t) {
    if (!t) return;
    pthread_mutex_destroy(&t->candado);
}

// Configura un vecino directo con su costo de enlace
void tabla_configurar_vecino(tabla_enrutamiento_t *t, int id_vecino, int costo, const char *ip, const char *nombre) {
    if (!t || id_vecino <= 0 || id_vecino >= MAX_ISLAS) return;
    pthread_mutex_lock(&t->candado);
    t->vecinos[id_vecino].id = id_vecino;
    t->vecinos[id_vecino].costo_enlace = costo;
    t->vecinos[id_vecino].configurado = 1;
    t->vecinos[id_vecino].ultimo_oido = 0;

    if (ip && ip[0] != '\0') {
        strncpy(t->ips[id_vecino], ip, sizeof(t->ips[id_vecino]) - 1);
        t->ips[id_vecino][sizeof(t->ips[id_vecino]) - 1] = '\0';
    }
    if (nombre && nombre[0] != '\0') {
        strncpy(t->nombres[id_vecino], nombre, sizeof(t->nombres[id_vecino]) - 1);
        t->nombres[id_vecino][sizeof(t->nombres[id_vecino]) - 1] = '\0';
    }

    pthread_mutex_unlock(&t->candado);
}

// Registra los datos de una isla
void tabla_configurar_isla(tabla_enrutamiento_t *t, int id_isla, const char *ip, const char *nombre) {
    if (!t || id_isla <= 0 || id_isla >= MAX_ISLAS) return;
    pthread_mutex_lock(&t->candado);
    if (ip && ip[0] != '\0') {
        strncpy(t->ips[id_isla], ip, sizeof(t->ips[id_isla]) - 1);
        t->ips[id_isla][sizeof(t->ips[id_isla]) - 1] = '\0';
    }
    if (nombre && nombre[0] != '\0') {
        strncpy(t->nombres[id_isla], nombre, sizeof(t->nombres[id_isla]) - 1);
        t->nombres[id_isla][sizeof(t->nombres[id_isla]) - 1] = '\0';
    }
    pthread_mutex_unlock(&t->candado);
}

// Consulta si la ruta es valida segun la tabla ROA del punto E
static int es_ruta_valida_roa(tabla_enrutamiento_t *t, int isla, time_t ahora) {
    if (isla <= 0 || isla >= MAX_ISLAS) return 0;
    (void)t;
    char ip[32];
    snprintf(ip, sizeof ip, "172.16.%d0.10", isla);

    roa_resultado_t res = roa_almacen_consultar(ip, isla, ahora);
    if (res == ROA_INVALID) {
        fprintf(stderr, "[ROA] Ruta descartada: isla %d no autorizada para IP %s\n", isla, ip);
        return 0;
    }
    return 1;
}

// Busca el mejor camino (en caso de empate gana el de menor id)
static int mejor_camino_sin_candado(const tabla_enrutamiento_t *t, int destino, int *costo_out, int *via_out) {
    int min_c = INF;
    int mejor_via = 0;

    for (int v = 1; v < MAX_ISLAS; v++) {
        if (!t->vecinos[v].configurado) continue;
        int c = t->matriz_costos[destino][v];
        if (c < min_c) {
            min_c = c;
            mejor_via = v;
        } else if (c == min_c && min_c < INF) {
            // Desempate por menor id
            if (mejor_via == 0 || v < mejor_via) {
                mejor_via = v;
            }
        }
    }

    if (costo_out) *costo_out = min_c;
    if (via_out) *via_out = mejor_via;
    return (min_c < INF && mejor_via > 0);
}

// Devuelve el mejor costo y por cual vecino ir
int tabla_obtener_mejor(tabla_enrutamiento_t *t, int destino, int *costo_out, int *via_out) {
    if (!t || destino <= 0 || destino >= MAX_ISLAS) return 0;
    pthread_mutex_lock(&t->candado);
    int ok = mejor_camino_sin_candado(t, destino, costo_out, via_out);
    pthread_mutex_unlock(&t->candado);
    return ok;
}

// Retorna el siguiente salto hacia el destino
int tabla_siguiente_salto(tabla_enrutamiento_t *t, int destino) {
    int via = 0;
    int costo = INF;
    if (tabla_obtener_mejor(t, destino, &costo, &via)) {
        return via;
    }
    return 0;
}

// Devuelve un resumen con las mejores rutas
void tabla_obtener_resumen(tabla_enrutamiento_t *t, ruta_optima_t *resumen_out, size_t *n_out) {
    if (!t || !resumen_out || !n_out) return;
    pthread_mutex_lock(&t->candado);

    size_t count = 0;
    for (int d = 1; d < MAX_ISLAS; d++) {
        if (d == t->mi_id) continue;
        int costo = INF, via = 0;
        if (mejor_camino_sin_candado(t, d, &costo, &via)) {
            resumen_out[count].destino = d;
            resumen_out[count].costo = costo;
            resumen_out[count].prox_salto = via;
            count++;
        }
    }
    *n_out = count;
    pthread_mutex_unlock(&t->candado);
}

// Compara si la tabla cambio respecto a una version anterior
static int resumen_ha_cambiado(const int antes_c[MAX_ISLAS], const int antes_v[MAX_ISLAS], const tabla_enrutamiento_t *t) {
    for (int d = 1; d < MAX_ISLAS; d++) {
        int ahora_c, ahora_v;
        mejor_camino_sin_candado(t, d, &ahora_c, &ahora_v);
        if (ahora_c != antes_c[d] || ahora_v != antes_v[d]) return 1;
    }
    return 0;
}

// Marca como inalcanzables las rutas que pasaban por el vecino
int tabla_vecino_caido(tabla_enrutamiento_t *t, int vecino) {
    if (!t || vecino <= 0 || vecino >= MAX_ISLAS) return 0;
    pthread_mutex_lock(&t->candado);

    int antes_c[MAX_ISLAS], antes_v[MAX_ISLAS];
    for (int d = 1; d < MAX_ISLAS; d++) {
        mejor_camino_sin_candado(t, d, &antes_c[d], &antes_v[d]);
    }

    t->vecinos[vecino].ultimo_oido = 0;
    for (int d = 1; d < MAX_ISLAS; d++) {
        t->matriz_costos[d][vecino] = INF;
    }

    int cambio = resumen_ha_cambiado(antes_c, antes_v, t);
    pthread_mutex_unlock(&t->candado);
    return cambio;
}

// Detecta si algun vecino supero el tiempo de silencio
int tabla_revisar_silencio(tabla_enrutamiento_t *t, int tolerancia, time_t ahora) {
    if (!t) return 0;
    if (ahora == 0) ahora = time(NULL);
    int hubo_cambio = 0;

    pthread_mutex_lock(&t->candado);
    for (int v = 1; v < MAX_ISLAS; v++) {
        if (!t->vecinos[v].configurado) continue;
        if (t->vecinos[v].ultimo_oido > 0 && (ahora - t->vecinos[v].ultimo_oido) > tolerancia) {
            int antes_c[MAX_ISLAS], antes_v[MAX_ISLAS];
            for (int d = 1; d < MAX_ISLAS; d++) {
                mejor_camino_sin_candado(t, d, &antes_c[d], &antes_v[d]);
            }

            for (int d = 1; d < MAX_ISLAS; d++) {
                t->matriz_costos[d][v] = INF;
            }
            t->vecinos[v].ultimo_oido = 0;

            if (resumen_ha_cambiado(antes_c, antes_v, t)) {
                hubo_cambio = 1;
            }
        }
    }
    pthread_mutex_unlock(&t->candado);
    return hubo_cambio;
}

// Procesa el vector de distancias recibido de un vecino
int tabla_recibir_vector(tabla_enrutamiento_t *t, int vecino, const int *destinos, const int *costos, size_t n_entradas, time_t ahora) {
    if (!t || vecino <= 0 || vecino >= MAX_ISLAS || !t->vecinos[vecino].configurado) return 0;
    if (ahora == 0) ahora = time(NULL);

    // 1. Revisa si es aviso de muerte (-1 hacia si mismo)
    for (size_t i = 0; i < n_entradas; i++) {
        if (destinos[i] == vecino && costos[i] == -1) {
            return tabla_vecino_caido(t, vecino);
        }
    }

    // 2. Valida ROA del vecino emisor
    if (!es_ruta_valida_roa(t, vecino, ahora)) {
        return 0;
    }

    pthread_mutex_lock(&t->candado);

    int antes_c[MAX_ISLAS], antes_v[MAX_ISLAS];
    for (int d = 1; d < MAX_ISLAS; d++) {
        mejor_camino_sin_candado(t, d, &antes_c[d], &antes_v[d]);
    }

    // 3. Limpia rutas viejas de este vecino y actualiza enlace directo
    for (int d = 1; d < MAX_ISLAS; d++) {
        if (d != vecino) t->matriz_costos[d][vecino] = INF;
    }
    t->vecinos[vecino].ultimo_oido = ahora;
    t->matriz_costos[vecino][vecino] = t->vecinos[vecino].costo_enlace;
    t->destinos_vistos[vecino] = 1;

    // 4. Aplica Bellman-Ford con validacion ROA para cada destino
    for (size_t i = 0; i < n_entradas; i++) {
        int d = destinos[i];
        int c = costos[i];

        if (d <= 0 || d >= MAX_ISLAS || d == t->mi_id || d == vecino || c == 0) {
            continue;
        }
        if (c > 0 && !es_ruta_valida_roa(t, d, ahora)) {
            continue;
        }

        t->destinos_vistos[d] = 1;
        if (c < 0) {
            t->matriz_costos[d][vecino] = INF;
        } else {
            int nuevo_costo = t->vecinos[vecino].costo_enlace + c;
            t->matriz_costos[d][vecino] = (nuevo_costo >= INF) ? INF : nuevo_costo;
        }
    }

    int cambio = resumen_ha_cambiado(antes_c, antes_v, t);
    pthread_mutex_unlock(&t->candado);
    return cambio;
}

// Arma el vector que se le manda a un vecino con poison reverse
size_t tabla_armar_vector_para(tabla_enrutamiento_t *t, int vecino, int *destinos_out, int *costos_out, size_t max_out) {
    if (!t || vecino <= 0 || vecino >= MAX_ISLAS) return 0;
    pthread_mutex_lock(&t->candado);

    size_t count = 0;
    for (int d = 1; d < MAX_ISLAS; d++) {
        if (d == vecino) continue;
        int costo = INF, via = 0;
        mejor_camino_sin_candado(t, d, &costo, &via);

        if (costo == INF && via == 0) continue;

        if (count < max_out) {
            destinos_out[count] = d;
            // Poison reverse: si paso por ese vecino, le anuncio -1
            if (costo >= INF || (POISON_REVERSE && via == vecino)) {
                costos_out[count] = -1;
            } else {
                costos_out[count] = costo;
            }
            count++;
        }
    }
    pthread_mutex_unlock(&t->candado);
    return count;
}

// Convierte las rutas en lineas ANNOUNCE|destino|costo
int tabla_armar_announce_texto(tabla_enrutamiento_t *t, int vecino, int muerte, char *buffer_out, size_t max_len) {
    if (!t || !buffer_out || max_len == 0) return -1;
    buffer_out[0] = '\0';

    if (muerte) {
        snprintf(buffer_out, max_len, "ANNOUNCE|%d|-1\n", t->mi_id);
        return 0;
    }

    int destinos[MAX_ISLAS];
    int costos[MAX_ISLAS];
    size_t n = tabla_armar_vector_para(t, vecino, destinos, costos, MAX_ISLAS);

    if (n == 0) {
        // Keepalive con costo 0
        snprintf(buffer_out, max_len, "ANNOUNCE|%d|0\n", t->mi_id);
        return 0;
    }

    size_t escrito = 0;
    for (size_t i = 0; i < n; i++) {
        int r = snprintf(buffer_out + escrito, max_len - escrito, "ANNOUNCE|%d|%d\n", destinos[i], costos[i]);
        if (r < 0 || (size_t)r >= (max_len - escrito)) break;
        escrito += r;
    }
    return 0;
}

// Convierte texto de ANNOUNCE a arreglos de destinos y costos
size_t tabla_parsear_announce_texto(const char *texto, int *destinos_out, int *costos_out, size_t max_entradas) {
    if (!texto || !destinos_out || !costos_out || max_entradas == 0) return 0;

    size_t count = 0;
    const char *linea = texto;

    while (*linea && count < max_entradas) {
        const char *fin_linea = strchr(linea, '\n');
        char buf[128];
        size_t len = fin_linea ? (size_t)(fin_linea - linea) : strlen(linea);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, linea, len);
        buf[len] = '\0';

        char comando[32];
        int d = 0, c = 0;
        if (sscanf(buf, "%31[^|]|%d|%d", comando, &d, &c) == 3) {
            for (char *p = comando; *p; p++) *p = (char)toupper((unsigned char)*p);
            if (strcmp(comando, "ANNOUNCE") == 0) {
                destinos_out[count] = d;
                costos_out[count] = c;
                count++;
            }
        }
        if (!fin_linea) break;
        linea = fin_linea + 1;
    }
    return count;
}

// Procesa paquetes DATA: entrega, decrementa TTL o reenvia
int tabla_procesar_data(tabla_enrutamiento_t *t, const char *msg_data, char *accion_out, int *salto_out, char *nuevo_msg_out, size_t max_len) {
    if (!t || !msg_data || !accion_out || !nuevo_msg_out) return -1;

    char prefijo[32], datos[1024];
    int origen = 0, destino = 0, ttl = 0;

    int leidos = sscanf(msg_data, "%31[^|]|%d|%d|%d|%1023[^\n]", prefijo, &origen, &destino, &ttl, datos);
    if (leidos < 5 || strcmp(prefijo, "DATA") != 0) {
        strcpy(accion_out, "DESCARTAR");
        if (salto_out) *salto_out = 0;
        snprintf(nuevo_msg_out, max_len, "Formato DATA invalido");
        return 0;
    }

    // 1. Mensaje para esta isla
    if (destino == t->mi_id) {
        strcpy(accion_out, "ENTREGAR");
        if (salto_out) *salto_out = 0;
        snprintf(nuevo_msg_out, max_len, "%s", datos);
        return 0;
    }

    // 2. Decrementar TTL
    ttl--;
    if (ttl <= 0) {
        strcpy(accion_out, "DESCARTAR");
        if (salto_out) *salto_out = 0;
        snprintf(nuevo_msg_out, max_len, "TTL agotado (origen=%d, destino=%d)", origen, destino);
        return 0;
    }

    // 3. Buscar siguiente salto
    int salto = tabla_siguiente_salto(t, destino);
    if (salto == 0) {
        strcpy(accion_out, "DESCARTAR");
        if (salto_out) *salto_out = 0;
        snprintf(nuevo_msg_out, max_len, "Sin ruta hacia isla %d (origen=%d)", destino, origen);
        return 0;
    }

    // 4. Reenviar con nuevo TTL
    strcpy(accion_out, "REENVIAR");
    if (salto_out) *salto_out = salto;
    snprintf(nuevo_msg_out, max_len, "DATA|%d|%d|%d|%s", origen, destino, ttl, datos);
    return 0;
}

// Imprime la tabla de enrutamiento con fecha y hora
void tabla_imprimir(tabla_enrutamiento_t *t) {
    if (!t) return;
    pthread_mutex_lock(&t->candado);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf_time[32];
    strftime(buf_time, sizeof(buf_time), "%Y-%m-%d %H:%M:%S", tm_info);

    const char *mi_nombre = t->nombres[t->mi_id];
    if (mi_nombre[0]) {
        printf("[%s] Tabla de isla %d (%s):\n", buf_time, t->mi_id, mi_nombre);
    } else {
        printf("[%s] Tabla de isla %d:\n", buf_time, t->mi_id);
    }

    for (int d = 1; d < MAX_ISLAS; d++) {
        if (d == t->mi_id || !t->destinos_vistos[d]) continue;
        int costo = INF, via = 0;
        mejor_camino_sin_candado(t, d, &costo, &via);

        const char *nom_d = t->nombres[d];
        char etiq_d[64];
        if (nom_d[0]) snprintf(etiq_d, sizeof(etiq_d), "isla %d (%s)", d, nom_d);
        else snprintf(etiq_d, sizeof(etiq_d), "isla %d", d);

        if (via > 0 && costo < INF) {
            const char *nom_via = t->nombres[via];
            char etiq_via[64];
            if (nom_via[0]) snprintf(etiq_via, sizeof(etiq_via), "isla %d (%s)", via, nom_via);
            else snprintf(etiq_via, sizeof(etiq_via), "isla %d", via);

            printf("  -> %s: costo %d, por %s\n", etiq_d, costo, etiq_via);
        } else {
            printf("  -> %s: INALCANZABLE\n", etiq_d);
        }
    }
    pthread_mutex_unlock(&t->candado);
}

// Carga vecinos e IPs desde config_koopatroopas.json
int tabla_cargar_config_json(tabla_enrutamiento_t *t, const char *ruta_json) {
    FILE *f = fopen(ruta_json, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    size_t leidos = fread(buf, 1, len, f);
    buf[leidos] = '\0';
    fclose(f);

    // Leer mi_id
    char *p = strstr(buf, "\"mi_id\"");
    int mi_id = 1;
    if (p) {
        sscanf(p, "\"mi_id\"%*[^0-9]%d", &mi_id);
    }

    // Leer puerto TCP
    int puerto = PUERTO_TCP_DEFECTO;
    p = strstr(buf, "\"puerto_tcp\"");
    if (p) {
        sscanf(p, "\"puerto_tcp\"%*[^0-9]%d", &puerto);
    }

    tabla_iniciar(t, mi_id, puerto);

    // Leer nombre propio
    p = strstr(buf, "\"nombre\"");
    if (p) {
        char nombre_propio[32];
        if (sscanf(p, "\"nombre\"%*[^\"]\"%31[^\"]\"", nombre_propio) == 1) {
            snprintf(t->nombres[mi_id], sizeof(t->nombres[mi_id]), "%s", nombre_propio);
        }
    }

       // Leer vecinos directos: isla, IP en la VLAN de transito y costo
    char *vec_sec = strstr(buf, "\"vecinos\"");
    if (vec_sec) {
        for (int id = 1; id < MAX_ISLAS; id++) {
            char patron[32];
            snprintf(patron, sizeof(patron), "\"%d\"", id);
            char *pos = strstr(vec_sec, patron);
            if (pos) {
                char ip[32] = {0}, equipo[32] = {0};
                int costo = 1;
                char *pos_ip = strstr(pos, "\"ip\"");
                char *pos_eq = strstr(pos, "\"equipo\"");
                char *pos_costo = strstr(pos, "\"costo\"");
                if (pos_ip) sscanf(pos_ip, "\"ip\"%*[^\"]\"%31[^\"]\"", ip);
                if (pos_eq) sscanf(pos_eq, "\"equipo\"%*[^\"]\"%31[^\"]\"", equipo);
                if (pos_costo) sscanf(pos_costo, "\"costo\"%*[^0-9]%d", &costo);
                tabla_configurar_vecino(t, id, costo, ip, equipo);
            }
        }
    }

    free(buf);
    return 0;
}
