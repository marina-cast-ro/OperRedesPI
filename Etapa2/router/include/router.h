#ifndef ROUTER_H
#define ROUTER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "forwarding.h"  // Contiene a processPacket() para el procesamiento 
                         // de tramas de los sockets (FD) vecinos
#include "mmu.h"
#include "routingTable.h"

#define ERROR_ROUTER      -1   // Bandera para indicar que el router contiene errores
#define MAX_BUFFER_SIZE 1024   // Tamaño (carácteres) máximo de los datos a enviar/recibir
#define MAX_PEERS         24   // Máximo de vecinos (basado en las personas aka I.P.'s del aula)

#define MAX_CONFIG_NEIGHBORS 8   // Routers vecinos que se pueden anotar en config.txt

// Struct de datos atributos del router
typedef struct {
    uint32_t localIp;   // IP del router en formato binario (4 bytes)
    int port;           // Puerto de entrada de datos al router
    int neighborCount;                          // Cuántos routers vecinos trae el config.txt
    char neighborIp[MAX_CONFIG_NEIGHBORS][16];  // IP real (la del wifi) de cada router vecino
    int neighborPort[MAX_CONFIG_NEIGHBORS];     // Puerto de cada router vecino
} ConfigRouter;

// Devuelve los sockets de los vecinos conectados.
// Retorna la cantidad de sockets devueltos.
// Si sockets es NULL o maxSockets es 0 o negativo, retorna 0.
int getNeighborSockets(int *sockets, int maxSockets);

// Inicia el socket de escucha en router.port y lanza el hilo pasivo en segundo plano.
// Retorna 0 si el servidor arrancó bien, -1 en caso de error.
int initRouterListen(ConfigRouter router);

// Función ejecutada por el hilo de escucha (compatible con pthread_create):
// - Ejecuta accept(): al conectar el Host/listener, extrae su IP y actualiza la MMU con saveRoute(ip_pc, client_fd).
// - Lee tramas entrantes desde los sockets y las pasa a processPacket(buffer, sockfd).
void *listening(void *arg);

// Detiene de forma limpia el hilo de escucha y cierra los sockets abiertos.
void endRouterListen(ConfigRouter router);

int getUdpSocket(void);
int getNeighborCount(void);
int getNeighborInfo(int index, char *outIp, int *outPort);

#endif  // ROUTER_H