#ifndef FRAMEBUILDER_H
#define FRAMEBUILDER_H

#include "protocol.h"

// --- CONSTRUIR TRAMAS ---
size_t buildFrame(uint8_t frameType, uint8_t currentSeq, const uint8_t *payloadBuffer, uint16_t payloadSize, uint8_t *outFrameBuffer);
size_t buildEndFrame(uint8_t currentSeq, uint8_t *outFrameBuffer);

// --- ENVIAR TRAMAS ---
int sendFrameSockets(const uint8_t *frameBuffer, size_t frameSize);

#endif