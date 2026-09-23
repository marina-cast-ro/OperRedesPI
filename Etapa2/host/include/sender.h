#ifndef SENDER_H
#define SENDER_H

#include "protocol.h"

// Construye una trama de datos usando protocol.h ("DATA|ipDestino|mensaje")
// y la envía al router asignado.
int sendMessage(const char *routerIp, int routerPort, const char *destIp, const char *message);

#endif