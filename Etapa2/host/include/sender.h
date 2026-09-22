#ifndef SENDER_H
#define SENDER_H

// Funcion encargada de dar el saludo: "Hola, esta es mi IP"
int greetAndMeet(const char *routerIP, int routerPort, const char *ownIP);

// Permite enviar un mensaje hacia el router, este se lo redirige hacia el destino
int sendMessage(const char *routerIP, int routerPort, const char *destIP, int destPort, const char *message);

#endif