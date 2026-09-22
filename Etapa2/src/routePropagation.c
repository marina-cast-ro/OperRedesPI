#include "routePropagation.h"

void initRouteTable(RouteTable *table) {
    if (table != NULL) {
        *table = (RouteTable){0};
    }
}

int receiveRoute(RouteTable *table, uint32_t destination,
                 uint32_t neighbor, unsigned int interfaceId) {
    if (table == NULL || table->count > MAX_ROUTES) {
        return ROUTE_ERROR;
    }

    // Buscar si ya conocemos este destino para no duplicar entradas.
    for (size_t i = 0; i < table->count; ++i) {
        const Route *route = &table->routes[i];
        if (route->destination == destination) {
            if (route->nextHop == neighbor && route->interfaceId == interfaceId) {
                return ROUTE_UNCHANGED;
            }

            // TODO falta escoger protocolo para elegir entre rutas alternativas.
            // Por ahora conservamos la ruta existente.
            return ROUTE_PENDING;
        }
    }

    if (table->count == MAX_ROUTES) {
        return ROUTE_ERROR;
    }

    // Registro provisional del destino a traves del vecino que lo anuncio.
    // TODO falta escoger protocolo para validar anuncios y evitar bucles.
    // TODO companeros del Modulo 2: guardar la ruta mediante su interfaz MMU.
    table->routes[table->count++] = (Route){destination, neighbor, interfaceId};
    return ROUTE_ADDED;
}

int propagateRoutes(const RouteTable *table) {
    if (table == NULL || table->count > MAX_ROUTES) {
        return ROUTE_ERROR;
    }

    // TODO falta escoger protocolo: formato, que rutas anunciar y a que vecinos.
    // TODO companero de sockets: conectar el envio de anuncios a los vecinos.
    // TODO Modulo 2: obtener las rutas desde la memoria simulada.
    // TODO falta escoger protocolo para actualizar o retirar rutas al perder vecinos.
    return ROUTE_PENDING;
}
