#ifndef VECTOR_DISTANCIA_H
#define VECTOR_DISTANCIA_H

#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include "../puntoE/roa_tabla.h"
#include "../mmu/mmu.h"

// Constantes acordadas para la red
#define MAX_ISLAS            32
#define INF                  100   // Costo maximo para inalcanzable
#define POISON_REVERSE       1     // Evita bucles de enrutamiento
#define TTL_INICIAL          8     // Saltos maximos para paquetes DATA
#define INTERVALO_ANUNCIO    30    // Envio de announce cada 30s
#define TOLERANCIA_SILENCIO  90    // Tiempo para dar un vecino por muerto
#define PUERTO_TCP_DEFECTO   5005  // Puerto entre routers

// Enlace directo con un vecino
typedef struct {
    int id;
    int costo_enlace;        // Costo del enlace directo
    time_t ultimo_oido;      // Ultima vez que envio announce
    int configurado;         // 1 si es vecino directo
} vecino_info_t;

// Mejor ruta hacia un destino
typedef struct {
    int destino;
    int costo;               // Mejor costo
    int prox_salto;          // Siguiente router (0 si no hay)
} ruta_optima_t;

// Tabla de enrutamiento principal
typedef struct {
    int mi_id;
    int puerto_tcp;
    
    char ips[MAX_ISLAS][16];
    char nombres[MAX_ISLAS][32];
    vecino_info_t vecinos[MAX_ISLAS];
    
    // Matriz de costos [destino][via_vecino]: vive en la memoria simulada de la Etapa 2
    // (1 byte por celda, direccion = destino*MAX_ISLAS + via). Solo se toca con celda_leer/celda_escribir,
    // siempre con 'candado' tomado (incluso para leer: la TLB se modifica en un Miss).
    mmu_t mmu;
    int destinos_vistos[MAX_ISLAS];
    
    // Candado para acceso seguro entre hilos
    pthread_mutex_t candado;
} tabla_enrutamiento_t;

// Inicializacion y configuracion
void tabla_iniciar(tabla_enrutamiento_t *t, int mi_id, int puerto);
void tabla_destruir(tabla_enrutamiento_t *t);
void tabla_configurar_vecino(tabla_enrutamiento_t *t, int id_vecino, int costo, const char *ip, const char *nombre);
void tabla_configurar_isla(tabla_enrutamiento_t *t, int id_isla, const char *ip, const char *nombre);
int  tabla_cargar_config_json(tabla_enrutamiento_t *t, const char *ruta_json);

// Consultas de rutas
int  tabla_obtener_mejor(tabla_enrutamiento_t *t, int destino, int *costo_out, int *via_out);
int  tabla_siguiente_salto(tabla_enrutamiento_t *t, int destino);
void tabla_obtener_resumen(tabla_enrutamiento_t *t, ruta_optima_t *resumen_out, size_t *n_out);

// Actualizaciones Bellman-Ford
int  tabla_recibir_vector(tabla_enrutamiento_t *t, int vecino, const int *destinos, const int *costos, size_t n_entradas, time_t ahora);
int  tabla_vecino_caido(tabla_enrutamiento_t *t, int vecino);
int  tabla_revisar_silencio(tabla_enrutamiento_t *t, int tolerancia, time_t ahora);

// Manejo de mensajes de texto
size_t tabla_armar_vector_para(tabla_enrutamiento_t *t, int vecino, int *destinos_out, int *costos_out, size_t max_out);
int    tabla_armar_announce_texto(tabla_enrutamiento_t *t, int vecino, int muerte, char *buffer_out, size_t max_len);
size_t tabla_parsear_announce_texto(const char *texto, int *destinos_out, int *costos_out, size_t max_entradas);

// Procesamiento de paquetes DATA
int  tabla_procesar_data(tabla_enrutamiento_t *t, const char *msg_data, char *accion_out, int *salto_out, char *nuevo_msg_out, size_t max_len);

// Mostrar tabla
void tabla_imprimir(tabla_enrutamiento_t *t);

// Imprime aciertos/fallos de TLB y fallos de pagina de la MMU de esta tabla (punto 4)
void tabla_imprimir_estadisticas_mmu(tabla_enrutamiento_t *t);

#endif
