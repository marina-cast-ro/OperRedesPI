#ifndef FRAMEBUILDER_H
#define FRAMEBUILDER_H

#include "protocol.h"

// --- CONSTRUIR TRAMAS ---
// Recibe maxOutBufferSize para evitar buffer overflow en User Space
size_t buildFrame(uint8_t frameType, const uint8_t *payloadBuffer, uint16_t payloadSize, 
                   uint8_t *outFrameBuffer, size_t maxOutBufferSize);

size_t buildEndFrame(uint8_t *outFrameBuffer, size_t maxOutBufferSize);

// --- ENVIAR TRAMAS ---
int sendFrameSockets(const char *ip, int port, const uint8_t *frameBuffer, size_t frameSize);

#endif // FRAMEBUILDER_H