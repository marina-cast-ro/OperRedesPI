#ifndef LISTENER_H
#define LISTENER_H

#include "protocol.h"

// Bucle en segundo plano para escuchar mensajes dirigidos a este nodo host.
// Usa decodeFrame() de protocol.h para parsear los paquetes entrantes.
int keepListening(int localPort);

// Imprime en pantalla el mensaje recibido ya deserializado.
void showMessage(const char* receivedMessage);

#endif