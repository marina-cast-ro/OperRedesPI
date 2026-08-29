#include "serverFrameBuilder.h"

// --- CONSTRUIR TRAMAS CON DATOS (Header + Payload) ---
// Construye la trama en 'outFrameBuffer' a partir del Header y el Payload.
// Retorna la cantidad total de bytes que mide la trama lista.
size_t buildFrame (uint8_t frameType, uint8_t currentSeq, const uint8_t *payloadBuffer, uint16_t payloadSize, uint8_t *outFrameBuffer) {
	if (!outFrameBuffer) return 0;
	
	Header header;
	header.type = frameType;
	header.seqNumber = currentSeq;
	header.payloadLength = payloadSize;

	// Copiar los 4 bytes del Header al inicio del buffer
	memcpy(outFrameBuffer, &header, sizeof(Header));	
	
	// Si hay datos utiles en el Payload, se copian justo luego del Header
	if (payloadBuffer && header.payloadLength > 0) {
		memcpy(outFrameBuffer + sizeof(Header), payloadBuffer, header.payloadLength);
	}

	return sizeof(Header) + header.payloadLength;
}

// --- CONSTRUIR TRAMA DE FIN (Trama tipo END) ---
size_t buildEndFrame(uint8_t currentSeq, uint8_t *outFrameBuffer) {
    return buildFrame(FRAME_END, currentSeq, NULL, 0, outFrameBuffer);
}

// --- ENVIAR TRAMAS ---
// Retorna: 0 si se envió correctamente, -1 si ocurrió un error en el socket/syscall.
int sendFrameSockets(const uint8_t *frameBuffer, size_t frameSize) {
    if (!frameBuffer || frameSize == 0) {
        return -1; // Error de parámetros
    }

    // PENDIENTE: Aqui va el envío de la trama usando Sockets

    return 0; // Éxito
}