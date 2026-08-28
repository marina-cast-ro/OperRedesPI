
#include "serverSendProtocol.h"

// --- MAIN DE PRUEBAS ---
int main (void) {	
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