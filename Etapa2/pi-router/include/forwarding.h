#ifndef FORWARDING_H
#define FORWARDING_H

// Punto de entrada principal para todo lo que llega por el socket en router.c
void processPacket(const char *buffer, int sockfd);

// Envía el mensaje ANNOUNCE propio por todos los sockets activos al encender el router
void sendInitialAnnounce(void);

#endif