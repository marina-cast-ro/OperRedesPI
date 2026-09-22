#ifndef ROUTER_H
#define ROUTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define ERROR_ROUTER      -1   // Bandera para indicar que el router contiene errores
#define MAX_BUFFER_SIZE 1024   // Tamaño (carácteres) máximo de los datos a enviar/recibir

// Struct de datos atributos del router
typedef struct {
    uint32_t localIp;   // IP del router en formato binario (4 bytes)
    int port;
} ConfigRouter;

// Configura el router leyendo config.txt:
// - Extrae la IP y puerto del router.
// - Precarga las rutas de los nodos locales directamente conectados en la MMU llamando a saveRoute().
// Retorna la estructura con la configuración o port = ERROR_ROUTER si falla.
ConfigRouter initRouter(void);

// Inicia el hilo (pthread) de escucha en segundo plano en el puerto indicado.
// Retorna 0 si el servidor arrancó bien, -1 en caso de error.
int initRouterListen(ConfigRouter router);

// Función ejecutada por el hilo de escucha (compatible con pthread_create):
// - Recibe tramas por el socket.
// - Invoca decodeFrame() para parsear.
// - Si es ANNOUNCE/ADVERTISE -> actualiza la MMU con saveRoute().
// - Si es DATA -> consulta la MMU con findRoute() y reenvía.
// void *listening(void *arg);

// Detiene de forma limpia el hilo de escucha y cierra los sockets abiertos.
void endRouterListen(ConfigRouter router);

#endif  // ROUTER_H