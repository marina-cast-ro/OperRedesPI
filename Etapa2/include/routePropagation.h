#ifndef ROUTE_PROPAGATION_H
#define ROUTE_PROPAGATION_H

#include <stddef.h>
#include <stdint.h>

#define MAX_ROUTES 32

// Borrador interno: no es el formato de los mensajes que viajaran por sockets.
// TODO falta escoger protocolo y confirmar los campos de cada ruta.
typedef struct {
    uint32_t destination;  // IPv4 en orden de red.
    uint32_t nextHop;      // IPv4 del vecino en orden de red.
    unsigned int interfaceId;  // Interfaz local por donde llego el anuncio.
} Route;

// Tabla temporal para avanzar sin la MMU. No es la memoria simulada final.
typedef struct {
    Route routes[MAX_ROUTES];
    size_t count;
} RouteTable;

enum {
    ROUTE_PENDING = -2,
    ROUTE_ERROR = -1,
    ROUTE_UNCHANGED = 0,
    ROUTE_ADDED = 1
};

void initRouteTable(RouteTable *table);

// Recibe datos ya extraidos del anuncio por la parte de sockets.
// Retorna ADDED, UNCHANGED, PENDING (ruta alternativa) o ERROR.
int receiveRoute(RouteTable *table, uint32_t destination,
                 uint32_t neighbor, unsigned int interfaceId);

// Por ahora retorna PENDING: aun no envia anuncios por la red.
int propagateRoutes(const RouteTable *table);

#endif
