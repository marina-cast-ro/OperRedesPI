#ifndef ROUTER_H
#define ROUTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
<<<<<<< HEAD
#include <stdbool.h>
=======
>>>>>>> main
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define ERROR_ROUTER      -1   // Bandera para indicar que el router contiene errores
#define MAX_BUFFER_SIZE 1024   // Tamaño (carácteres) máximo de los datos a enviar/recibir

<<<<<<< HEAD
extern int sock_fd;  // Socket global responsable de la escucha

// Struct de los datos atributos de un vecino
typedef struct {
    char id[MAX_NAME_ROUTER];
    char ip[16];
    int port;
} Peer;

=======
>>>>>>> main
// Struct de datos atributos del router
typedef struct {
    uint32_t localIp;   // IP del router en formato binario (4 bytes)
    int port;
} ConfigRouter;

// Inicia el socket de escucha en router.port y lanza el hilo pasivo en segundo plano.
// Retorna 0 si el servidor arrancó bien, -1 en caso de error.
int initRouterListen(ConfigRouter router);

// Función ejecutada por el hilo de escucha (compatible con pthread_create):
// - Ejecuta accept(): al conectar el Host/listener, extrae su IP y actualiza la MMU con saveRoute(ip_pc, client_fd).
// - Lee tramas entrantes desde los sockets y las pasa a processPacket(buffer, sockfd).
// === DESCOMENTAR CUANDO LA FUNCION DE HILO ESTE LISTA ===
// void *listening(void *arg);

// Detiene de forma limpia el hilo de escucha y cierra los sockets abiertos.
void endRouterListen(ConfigRouter router);

#endif  // ROUTER_H