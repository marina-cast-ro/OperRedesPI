#ifndef ROUTER_H
#define ROUTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_NAME_ROUTER   50   // Tamaño (carácteres) máximo del nombre del router
#define MAX_PEERS         10   // Valor máximo de vecinos que puede tener el router
#define ERROR_ROUTER      -1   // Bandera para indicar que el router contiene errores
#define MAX_BUFFER_SIZE 1024   // Tamaño (carácteres) máximo de los datos a enviar/recibir

extern int sock_fd;  // Socket global responsable de la escucha

// Struct de los datos atributos de un vecino
typedef struct {
    char id[MAX_NAME_ROUTER];
    char ip[16];
    int port;
} Peer;

// Struct de datos atributos del router
typedef struct {
    char id[MAX_NAME_ROUTER];
    int port;
    char ip[16];
    int num_peers;
    Peer peers[MAX_PEERS];
} ConfigRouter;

// Configura el router con la información de config.txt
ConfigRouter initRouter(void);

// Router escucha infinitamente mensajes entrantes
//static int listening(ConfigRouter router);  // VIEJA
void *listening(void *arg);                   // NUEVA

// Router comienza a escuchar
//static int initRouterListen(ConfigRouter router);  // VIEJO

// Router para de escuchar
//static void endRouterListen(ConfigRouter router);  // VIEJO

// Router envía mensaje inicial a los routers vecinos para indicar su activación y conexión
//static void sendInitialMsg(ConfigRouter router);
void sendInitialMsg(ConfigRouter router);

// Función principal para el llamado y activación del router
// Devuelve el hilo de escucha para mantenerlo activo desde main.c
pthread_t activateRouter(void);

#endif  // ROUTER_H