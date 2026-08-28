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

// --- ENVIAR TRAMAS ---
// Retorna: 0 si se envió correctamente, -1 si ocurrió un error en el socket/syscall.
size_t sendFrameSockets(const uint8_t *frameBuffer, size_t frameSize) {
    if (!frameBuffer || frameSize == 0) {
        return -1; // Error de parámetros
    }

    // ṔENDIENTE: Aqui va el envío de la trama usando Sockets

    return 0; // Éxito
}