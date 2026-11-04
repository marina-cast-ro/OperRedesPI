#define _POSIX_C_SOURCE 200809L
#include "vector_distancia.h"
#include "../puntoE/roa_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static tabla_enrutamiento_t g_tabla;
static volatile sig_atomic_t g_ejecutando = 1;
static int g_socket_servidor = -1;

// Envia un mensaje TCP al router vecino con timeout de 5s
static int enviar_tcp(const char *ip, int puerto, const char *texto) {
    if (!ip || ip[0] == '\0') return -1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)puerto);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1;
    }

    send(sock, texto, strlen(texto), 0);
    close(sock);
    return 0;
}

// Envia el anuncio de rutas a todos los vecinos directos
static void anunciar_a_vecinos(tabla_enrutamiento_t *t, int muerte) {
    char buffer[2048];
    for (int v = 1; v < MAX_ISLAS; v++) {
        if (!t->vecinos[v].configurado) continue;
        const char *ip = t->ips[v];
        if (ip[0] == '\0') continue;

        tabla_armar_announce_texto(t, v, muerte, buffer, sizeof(buffer));
        enviar_tcp(ip, t->puerto_tcp, buffer);
    }
}

// Hilo que atiende un paquete entrante (ANNOUNCE o DATA)
static void *hilo_conexion(void *arg) {
    int client_sock = (int)(intptr_t)arg;
    char buffer[4096];
    ssize_t leidos = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (leidos > 0) {
        buffer[leidos] = '\0';
        if (strncmp(buffer, "ANNOUNCE", 8) == 0) {
            // Identificar vecino por su IP
            struct sockaddr_in peer;
            socklen_t peer_len = sizeof(peer);
            getpeername(client_sock, (struct sockaddr *)&peer, &peer_len);
            char ip_remota[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer.sin_addr, ip_remota, sizeof(ip_remota));

            int id_vecino = 0;
            for (int v = 1; v < MAX_ISLAS; v++) {
                if (strcmp(g_tabla.ips[v], ip_remota) == 0) {
                    id_vecino = v;
                    break;
                }
            }

            int destinos[MAX_ISLAS];
            int costos[MAX_ISLAS];
            size_t n = tabla_parsear_announce_texto(buffer, destinos, costos, MAX_ISLAS);

            if (id_vecino > 0) {
                if (tabla_recibir_vector(&g_tabla, id_vecino, destinos, costos, n, time(NULL))) {
                    tabla_imprimir(&g_tabla);
                }
            }
        } else if (strncmp(buffer, "DATA", 4) == 0) {
            char accion[32], nuevo_msg[2048];
            int siguiente_salto = 0;

            tabla_procesar_data(&g_tabla, buffer, accion, &siguiente_salto, nuevo_msg, sizeof(nuevo_msg));

            if (strcmp(accion, "ENTREGAR") == 0) {
                printf("[DATA RECIBIDO] %s\n", nuevo_msg);
            } else if (strcmp(accion, "REENVIAR") == 0) {
                strcat(nuevo_msg, "\n");
                enviar_tcp(g_tabla.ips[siguiente_salto], g_tabla.puerto_tcp, nuevo_msg);
            } else {
                printf("[DATA DESCARTADO] %s\n", nuevo_msg);
            }
        }
    }

    close(client_sock);
    return NULL;
}

// Hilo para enviar anuncios cada 30s y verificar silencios de 90s
static void *hilo_temporizador(void *arg) {
    (void)arg;
    while (g_ejecutando) {
        sleep(INTERVALO_ANUNCIO);
        if (!g_ejecutando) break;

        anunciar_a_vecinos(&g_tabla, 0);
        if (tabla_revisar_silencio(&g_tabla, TOLERANCIA_SILENCIO, time(NULL))) {
            tabla_imprimir(&g_tabla);
        }
    }
    return NULL;
}

// Captura Ctrl+C para mandar aviso de muerte (-1) a los vecinos
static void manejador_senales(int sig) {
    (void)sig;
    printf("\nCerrando nodo y enviando aviso de muerte (-1) a los vecinos...\n");
    anunciar_a_vecinos(&g_tabla, 1);
    g_ejecutando = 0;
    if (g_socket_servidor >= 0) {
        close(g_socket_servidor);
        g_socket_servidor = -1;
    }
}

// Modo enrutador conectado a la red TCP real
static void ejecutar_en_red(const char *ruta_cfg) {
    if (tabla_cargar_config_json(&g_tabla, ruta_cfg) != 0) {
        fprintf(stderr, "No se pudo leer %s. Usando configuracion por defecto para Isla 1...\n", ruta_cfg);
        tabla_iniciar(&g_tabla, 1, PUERTO_TCP_DEFECTO);
        tabla_configurar_vecino(&g_tabla, 3, 5, "10.0.3.1", "4bits");
        tabla_configurar_vecino(&g_tabla, 4, 2, "10.0.4.1", "Pirateados");
        tabla_configurar_vecino(&g_tabla, 5, 2, "10.0.5.1", "Pi de Frambuesa");
    }

    // Inicializar almacen de ROAs
    roa_almacen_iniciar();

    signal(SIGINT, manejador_senales);
    signal(SIGTERM, manejador_senales);

    g_socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket_servidor < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(g_socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((uint16_t)g_tabla.puerto_tcp);

    if (bind(g_socket_servidor, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(g_socket_servidor);
        exit(1);
    }

    listen(g_socket_servidor, 16);
    printf("Enrutador C de isla %d escuchando en TCP puerto %d...\n", g_tabla.mi_id, g_tabla.puerto_tcp);

    pthread_t th_timer;
    pthread_create(&th_timer, NULL, hilo_temporizador, NULL);

    while (g_ejecutando) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(g_socket_servidor, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (!g_ejecutando) break;
            continue;
        }

        pthread_t th;
        pthread_create(&th, NULL, hilo_conexion, (void *)(intptr_t)client_sock);
        pthread_detach(th);
    }

    pthread_join(th_timer, NULL);
    roa_almacen_cerrar();
    tabla_destruir(&g_tabla);
    printf("Enrutador detenido exitosamente.\n");
}

// Simulacion de 5 islas para pruebas locales
static void ejecutar_simulacion(void) {
    printf("=== MODO SIMULACION: Topologia de 5 islas ===\n");
    roa_almacen_iniciar();

    tabla_enrutamiento_t nodos[6];
    for (int i = 1; i <= 5; i++) {
        tabla_iniciar(&nodos[i], i, PUERTO_TCP_DEFECTO);
    }

    const char *nombres[6] = {"", "Koopatroopas", "SHELL", "4bits", "Pirateados", "Pi de Frambuesa"};
    for (int i = 1; i <= 5; i++) {
        for (int j = 1; j <= 5; j++) {
            tabla_configurar_isla(&nodos[i], j, "", nombres[j]);
        }
    }

    // Configurar enlaces directos segun la matriz acordada
    // Isla 1 (Koopatroopas)
    tabla_configurar_vecino(&nodos[1], 3, 5, "", "4bits");
    tabla_configurar_vecino(&nodos[1], 4, 2, "", "Pirateados");
    tabla_configurar_vecino(&nodos[1], 5, 2, "", "Pi de Frambuesa");

    // Isla 2 (SHELL)
    tabla_configurar_vecino(&nodos[2], 3, 2, "", "4bits");
    tabla_configurar_vecino(&nodos[2], 5, 1, "", "Pi de Frambuesa");

    // Isla 3 (4bits)
    tabla_configurar_vecino(&nodos[3], 1, 5, "", "Koopatroopas");
    tabla_configurar_vecino(&nodos[3], 2, 2, "", "SHELL");
    tabla_configurar_vecino(&nodos[3], 4, 1, "", "Pirateados");

    // Isla 4 (Pirateados)
    tabla_configurar_vecino(&nodos[4], 1, 2, "", "Koopatroopas");
    tabla_configurar_vecino(&nodos[4], 3, 1, "", "4bits");
    tabla_configurar_vecino(&nodos[4], 5, 5, "", "Pi de Frambuesa");

    // Isla 5 (Pi de Frambuesa)
    tabla_configurar_vecino(&nodos[5], 1, 2, "", "Koopatroopas");
    tabla_configurar_vecino(&nodos[5], 2, 1, "", "SHELL");
    tabla_configurar_vecino(&nodos[5], 4, 5, "", "Pirateados");

    printf("\n=== 1. Convergencia inicial de la red (5 equipos) ===\n");
    for (int ronda = 0; ronda < 4; ronda++) {
        for (int emisor = 1; emisor <= 5; emisor++) {
            for (int vecino = 1; vecino <= 5; vecino++) {
                if (!nodos[emisor].vecinos[vecino].configurado) continue;

                int dests[MAX_ISLAS], costos[MAX_ISLAS];
                size_t n = tabla_armar_vector_para(&nodos[emisor], vecino, dests, costos, MAX_ISLAS);
                if (tabla_recibir_vector(&nodos[vecino], emisor, dests, costos, n, time(NULL)) && vecino == 1) {
                    tabla_imprimir(&nodos[1]);
                }
            }
        }
    }

    printf("\n=== 2. Prueba de reenvio DATA desde Koopatroopas (1) hacia SHELL (2) ===\n");
    char paquete[512] = "DATA|1|2|8|Hola SHELL desde Koopatroopas";
    int actual = 1;
    while (1) {
        char accion[32], nuevo[512];
        int salto = 0;
        tabla_procesar_data(&nodos[actual], paquete, accion, &salto, nuevo, sizeof(nuevo));
        printf("  Isla %d -> accion=%s, siguiente_salto=%d, info='%s'\n", actual, accion, salto, nuevo);
        if (strcmp(accion, "REENVIAR") != 0) break;
        strcpy(paquete, nuevo);
        actual = salto;
    }

    printf("\n=== 3. Caida de Pirateados (isla 4) con aviso de muerte ===\n");
    for (int v = 1; v <= 5; v++) {
        if (!nodos[4].vecinos[v].configurado) continue;
        int d[1] = {4}, c[1] = {-1};
        tabla_recibir_vector(&nodos[v], 4, d, c, 1, time(NULL));
    }

    while (1) {
        int cambio = 0;
        int participantes[4] = {1, 2, 3, 5};
        for (int i = 0; i < 4; i++) {
            int emisor = participantes[i];
            for (int j = 0; j < 4; j++) {
                int receptor = participantes[j];
                if (!nodos[emisor].vecinos[receptor].configurado) continue;

                int dests[MAX_ISLAS], costos[MAX_ISLAS];
                size_t n = tabla_armar_vector_para(&nodos[emisor], receptor, dests, costos, MAX_ISLAS);
                cambio |= tabla_recibir_vector(&nodos[receptor], emisor, dests, costos, n, time(NULL));
            }
        }
        if (!cambio) break;
    }
    tabla_imprimir(&nodos[1]);

    for (int i = 1; i <= 5; i++) {
        tabla_destruir(&nodos[i]);
    }
    roa_almacen_cerrar();
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--red") == 0 || strcmp(argv[1], "-r") == 0)) {
        const char *cfg = (argc > 2) ? argv[2] : "config_koopatroopas.json";
        ejecutar_en_red(cfg);
    } else {
        ejecutar_simulacion();
    }
    return 0;
}
