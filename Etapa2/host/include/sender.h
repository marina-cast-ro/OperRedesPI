#ifndef SENDER_H
#define SENDER_H

#include "protocol.h"

// Construye una trama de datos usando protocol.h ("DATA|ipDestino|mensaje")
// y la envía al router asignado.
int sendMessage(const char *routerIp, int routerPort, const char *destIp, const char *message);

// Lee un archivo .txt línea por línea y envía cada línea como una trama DATA
// al router asignado hacia la IP destino.
int sendFile(const char *routerIp, int routerPort, const char *destIp, const char *filePath);

#endif