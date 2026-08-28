#include "serverFrameBuilder.h"

// --- CONSTRUIR TRAMAS (Header + Payload) ---
// Construye la trama en 'outFrameBuffer' a partir del Header y el Payload.
// Retorna la cantidad total de bytes que mide la trama lista.
size_t buildFrame (const Header *header, const uint8_t *payloadBuffer, uint8_t *outFrameBuffer) {
	if (!header || !outFrameBuffer) return 0;

	// Copiar los 4 bytes del Header al inicio del buffer
	memcpy(outFrameBuffer, header, sizeof(Header));	
	
	// Si hay datos utiles en el Payload, se copian justo luego del Header
	if (payloadBuffer && header->payloadLength > 0) {
		memcpy(outFrameBuffer + sizeof(Header), payloadBuffer, header->payloadLength);
	}

	return sizeof(Header) + header->payloadLength;
}