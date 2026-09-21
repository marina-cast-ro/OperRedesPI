#ifndef ROUTER_H
#define ROUTER_H

#define MAX_NAME_ROUTER  50   // Tamaño (carácteres) máximo del nombre del router
#define MAX_PEERS        10   // Valor máximo de vecinos que puede tener el router
#define ERROR_ROUTER     -1   // Bandera para indicar que el router contiene errores

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

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

ConfigRouter initRouter(void);

#endif  // ROUTER_H