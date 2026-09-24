#ifndef LISTENER_H
#define LISTENER_H

#include "protocol.h"

// Se conecta al puerto de escucha del router local (routerPort)
// y mantiene el bucle recv() para recibir tramas DATA dirigidas a esta PC.
// Usa decodeFrame() para parsear los paquetes.
int keepListening(const char *routerIp, int routerPort);

// Imprime en pantalla el mensaje recibido ya deserializado.

#endif