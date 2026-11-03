#ifndef VECTOR_DISTANCIA_H
#define VECTOR_DISTANCIA_H

#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include "../puntoE/roa_tabla.h"

#define MAX_ISLAS            32
#define INF                  100   /* Costo >= 100 se considera inalcanzable (infinito) */
#define POISON_REVERSE       1     /* 1 para activar envenenamiento de ruta inversa */
#define TTL_INICIAL          8     /* Saltos maximos para paquetes DATA */
#define INTERVALO_ANUNCIO    30    /* Segundos entre envios periodicos de ANNOUNCE */
#define TOLERANCIA_SILENCIO  90    /* Segundos de inactividad para declarar vecino caido */
#define PUERTO_TCP_DEFECTO   5005  /* Puerto TCP estandar entre routers */

/* Informacion de enlace directo con un vecino */
typedef struct {
    int id;
    int costo_enlace;        /* Costo directo del cable */
    time_t ultimo_oido;      /* Timestamp del ultimo ANNOUNCE recibido */
    int configurado;         /* 1 si es vecino directo configurado */
} vecino_info_t;

/* Mejor ruta hacia un destino */
typedef struct {
    int destino;
    int costo;               /* Costo minimo acumulado */
    int prox_salto;          /* ID de la isla vecina (0 si inalcanzable) */
} ruta_optima_t;

/* Estructura central de la Tabla de Enrutamiento */
typedef struct {
    int mi_id;
    int puerto_tcp;
    
    /* Metadatos de la topologia */
    char ips[MAX_ISLAS][16];
    char nombres[MAX_ISLAS][32];
    vecino_info_t vecinos[MAX_ISLAS];
    
    /* Matriz bidimensional Bellman-Ford: matriz_costos[destino][via_vecino] */
    int matriz_costos[MAX_ISLAS][MAX_ISLAS];
    int destinos_vistos[MAX_ISLAS]; /* 1 si el destino ha sido registrado en la red */
    
    /* Candado para proteger concurrencia en entorno multihilo */
    pthread_mutex_t candado;
} tabla_enrutamiento_t;

/* --- Inicializacion y configuracion --- */
void tabla_iniciar(tabla_enrutamiento_t *t, int mi_id, int puerto);
void tabla_destruir(tabla_enrutamiento_t *t);
void tabla_configurar_vecino(tabla_enrutamiento_t *t, int id_vecino, int costo, const char *ip, const char *nombre);
void tabla_configurar_isla(tabla_enrutamiento_t *t, int id_isla, const char *ip, const char *nombre);
int  tabla_cargar_config_json(tabla_enrutamiento_t *t, const char *ruta_json);

/* --- Consultas --- */
int  tabla_obtener_mejor(tabla_enrutamiento_t *t, int destino, int *costo_out, int *via_out);
int  tabla_siguiente_salto(tabla_enrutamiento_t *t, int destino);
void tabla_obtener_resumen(tabla_enrutamiento_t *t, ruta_optima_t *resumen_out, size_t *n_out);

/* --- Actualizacion y Bellman-Ford --- */
int  tabla_recibir_vector(tabla_enrutamiento_t *t, int vecino, const int *destinos, const int *costos, size_t n_entradas, time_t ahora);
int  tabla_vecino_caido(tabla_enrutamiento_t *t, int vecino);
int  tabla_revisar_silencio(tabla_enrutamiento_t *t, int tolerancia, time_t ahora);

/* --- Generacion y formateo de mensajes --- */
size_t tabla_armar_vector_para(tabla_enrutamiento_t *t, int vecino, int *destinos_out, int *costos_out, size_t max_out);
int    tabla_armar_announce_texto(tabla_enrutamiento_t *t, int vecino, int muerte, char *buffer_out, size_t max_len);
size_t tabla_parsear_announce_texto(const char *texto, int *destinos_out, int *costos_out, size_t max_entradas);

/* --- Reenvio de paquetes DATA --- */
int  tabla_procesar_data(tabla_enrutamiento_t *t, const char *msg_data, char *accion_out, int *salto_out, char *nuevo_msg_out, size_t max_len);

/* --- Impresion y bitacora --- */
void tabla_imprimir(tabla_enrutamiento_t *t);

#endif /* VECTOR_DISTANCIA_H */
