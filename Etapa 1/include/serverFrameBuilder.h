#ifndef FRAMEBUILDER_H
#define FRAMEBUILDER_H

#include "protocol.h"


size_t buildFrame (const Header *header, const uint8_t *payloadBuffer, uint8_t *outFrameBuffer);

#endif