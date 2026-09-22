#ifndef LISTENER_H
#define LISTENER_H

//Permite escuchar los mensajes en la computadora de destino.
int keepListening(int localPort);

//Se encarga de imprimir en pantalla los mensajes recibidos
void showMessage(const char* receivedMessage);

#endif