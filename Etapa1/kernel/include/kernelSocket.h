#ifndef KERNELSOCKETH
#define KERNELSOCKETH

#include "protocol.h"

// === FIRMAS DE FUNCIONES ===

// Crear y cerrar socket UDP en Kernel Space
int ksocketCreate(struct socket **socket_out);
void ksocketRelease(struct socket *socket);

// Enviar y recibir datos a nivel de Kernel
int ksocket_sendto(struct socket *socket, const char *ip_dest, int port, const void *buffer, size_t length);
int ksocket_recvfrom(struct socket *socket, void *buffer, size_t length, long timeout_ms);

#endif