#ifndef STOPANDWAIT_H
#define STOPANDWAIT_H

#include "protocol.h"

// === FIRMAS DE FUNCIONES ===

// Inicializa o reinicia el bit de secuencia del Kernel (0 o 1
void initProtocolState(void);

// Inyecta el seqNumber actual en la trama, la envia por UDP, activa el TIMER y espera el ACK
int sendFrameStopAndWait(const char *ip_dest, int port, const uint8_t *frameData, size_t length);

#endif