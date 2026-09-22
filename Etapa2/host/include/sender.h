#ifndef SENDER_H
#define SENDER_H

#include "protocol.h"

// Envía un paquete "DATA|ipDestino|mensaje" desde el nodo host hacia el router asignado
int sendMessage(const char *routerIp, int routerPort, const char *destIp, const char *message);

// Construye una trama de datos usando protocol.h ("DATA|ipDestino|mensaje")
// y la envía al router asignado.
int sendMessage(const char *routerIp, int routerPort, const char *destIp, const char *message);

#endif