#include "serverFrameBuilder.h"

// --- CONSTRUIR TRAMAS CON DATOS (Header + Payload) ---
// Construye la trama en 'outFrameBuffer' a partir del Header y el Payload.
// Retorna la cantidad total de bytes que mide la trama lista.
size_t buildFrame (uint8_t frameType, const uint8_t *payloadBuffer, uint16_t payloadSize, 
					uint8_t *outFrameBuffer, size_t maxOutBufferSize) {

	// Verificación de punteros y protección contra Buffer Overflow
    if (!outFrameBuffer || (sizeof(Header) + payloadSize) > maxOutBufferSize) {
        return 0; 
    }
	
	Header header;
	header.type = frameType;
	header.seqNumber = 0; // Se deja en 0 (el Kernel inyectará el bit alternante)
	header.payloadLength = htons(payloadSize); // Orden de red (Big-Endian)

	// Copiar los 4 bytes del Header al inicio del buffer
	memcpy(outFrameBuffer, &header, sizeof(Header));	
	
	// Si hay datos utiles en el Payload, se copian justo luego del Header
	if (payloadBuffer && payloadSize > 0) {
		memcpy(outFrameBuffer + sizeof(Header), payloadBuffer, payloadSize);
	}

	return sizeof(Header) + payloadSize;
}

// --- CONSTRUIR TRAMA DE FIN (Trama tipo END) ---
size_t buildEndFrame(uint8_t *outFrameBuffer, size_t maxOutBufferSize) {
    return buildFrame(PROTOCOL_FRAME_END, NULL, 0, outFrameBuffer, maxOutBufferSize);
}

// --- ENVIAR TRAMAS ---
// Retorna: 0 si se envió correctamente, -1 si ocurrió un error en el socket/syscall.
int sendFrameSockets(const char *ip, int port, const uint8_t *frameBuffer, size_t frameSize) {
    if (!frameBuffer || frameSize == 0) {
        return -1; // Error de parámetros
    }

    /* -------------------------------------------------------------------
     * PARA INTEGRACIÓN (Descomentar cuando el módulo del Kernel esté listo)
     * -------------------------------------------------------------------
     * long result = syscall(SYS_UDP_RELIABLE, ip, port, frameBuffer, frameSize);
     * return (result == 0) ? 0 : -1;
     * ------------------------------------------------------------------- */

    // PARA PRUEBA LOCAL: Simula que el Kernel envió la trama y recibió el ACK con éxito
    return 0;
}