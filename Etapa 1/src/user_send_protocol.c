
#include "user_send_protocol.h"

// --- FUNCIONES RELACIONADAS LECTURA DE ARCHIVOS ---
// Inicializa la lectura del archivo 
FILE* openInputFile(const char *filePath) {
	FILE *file = fopen(filePath, "rb");	// rb: lectura binaria segura del archivo
	if (!file) {
		perror("Opening file failed");
	}
	return file;
}

// Lee el SIGUIENTE bloque de datos del archivo de texto.
// Retorna: Cantidad de bytes leídos reales (0 indica Fin de Archivo / EOF).
size_t readNextPayload(FILE *file, uint8_t *payloadBuffer) {
	if (!file || !payloadBuffer) return 0;

	// fread avanza automáticamente el puntero del archivo en cada llamada
    return fread(payloadBuffer, 1, MAX_PAYLOAD_SIZE, file);
}

// Cierra el archivo
void closeInputFile(FILE *file) {
    if (file) {
        fclose(file);
    }
}


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


// --- MAIN DE PRUEBAS ---
int main (int argc, char *argv[]) {	
	FILE* file = openInputFile("tests.txt");
	if (!file) return 1;

	FILE* outputFile = fopen("output.txt", "wb");
	if (!outputFile) {
        perror("Opening output file failed");
        closeInputFile(file);
        return 1;
    }

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	size_t payloadSize;
	uint8_t currentSeq = 0; // Bit de secuencia inicial en 0

	while((payloadSize = readNextPayload(file, buffer)) > 0) {
		fwrite(buffer, 1, payloadSize, outputFile);
			
		printf("Read bytes: %zu\n", payloadSize);

		Header header;
		header.type = FRAME_DATA;
		header.seqNumber = currentSeq;
		header.payloadLength = (uint16_t)payloadSize;

		uint8_t frameBuffer[sizeof(Header) + MAX_PAYLOAD_SIZE];

		size_t frameSize = buildFrame(&header, buffer, frameBuffer);
		printf("Frame size: %zu | Seq bit: %u\n\n", frameSize, header.seqNumber);

		// Simular que el ACK llegó con éxito: alternar el bit para el siguiente bloque
        currentSeq = 1 - currentSeq;
	}	

	printf("There's no more bytes to read!\n");

	closeInputFile(file);
	fclose(outputFile);
	return 0;
}