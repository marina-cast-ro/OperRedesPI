#ifndef LISTENER_H
#define LISTENER_H

#include "protocol.h"

// Se conecta al puerto de escucha del router local (routerPort)
// y mantiene el bucle recv() para recibir tramas DATA dirigidas a esta PC.
// Guarda los mensajes recibidos en outputFilePath (o en "output.txt" por defecto).
// Usa decodeFrame() para parsear los paquetes.
int keepListening(const char *routerIp, int routerPort, const char *hostLogicalIp, const char *outputFilePath);

// Imprime en pantalla el mensaje recibido ya deserializado.
void showMessage(const char* receivedMessage);

// Guarda una línea/mensaje en el archivo de texto especificado.
void saveMessageToFile(const char *filePath, const char *receivedMessage);

#endif