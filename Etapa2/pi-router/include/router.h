#ifndef ROUTER_H
#define ROUTER_H

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "../../../Etapa1/kernel/include/kernelSocket.h"

#define MAX_NAME_ROUTER   50   // Tamaño (carácteres) máximo del nombre del router
#define MAX_PEERS         10   // Valor máximo de vecinos que puede tener el router
#define ERROR_ROUTER      -1   // Bandera para indicar que el router contiene errores
#define MAX_BUFFER_SIZE 1024   // Tamaño (carácteres) máximo de los datos a enviar/recibir

// Structs necesarios para definir responsables de la escucha
static struct socket      *listen_socket = NULL;
static struct task_struct *listen_thread = NULL;

// Struct de los datos atributos de un vecino
typedef struct {
    char id[MAX_NAME_ROUTER];
    char ip[15];
    int port;
} Peer;

// Struct de datos atributos del router
typedef struct {
    char id[MAX_NAME_ROUTER];
    int port;
    int num_peers;
    Peer peers[MAX_PEERS];
} ConfigRouter;

// Configura el router con la información de config.txt
ConfigRouter initRouter(void);

// Router escucha infinitamente mensajes entrantes
static int listening(ConfigRouter router);

// Router comienza a escuchar
static int initRouterListen(ConfigRouter router);

// Router para de escuchar
static int endRouterListen(ConfigRouter router);

#endif  // ROUTER_H