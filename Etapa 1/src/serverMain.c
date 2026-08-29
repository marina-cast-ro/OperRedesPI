
#include "protocol.h"
#include "serverFileReader.h"
#include "serverFrameBuilder.h"

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

	uint8_t payloadBuffer[MAX_PAYLOAD_SIZE];
	size_t payloadSize;
	uint8_t currentSeq = 0; // Bit de secuencia inicial en 0

	while((payloadSize = readNextPayload(file, payloadBuffer)) > 0) {
		fwrite(payloadBuffer, 1, payloadSize, outputFile);
			
		printf("Read bytes: %zu\n", payloadSize);

		uint8_t frameBuffer[sizeof(Header) + MAX_PAYLOAD_SIZE];

		size_t frameSize = buildFrame(FRAME_DATA, currentSeq, payloadBuffer, payloadSize, frameBuffer);
		printf("Frame size: %zu | Seq bit: %u\n", frameSize, currentSeq);

		printf("Sending the frame...\n\n");
		sendFrameSockets(frameBuffer, frameSize);	// PENDIENTE: IMPLEMENTAR ESTA FUNCION
		

		// Simular que el ACK llegó con éxito: alternar el bit para el siguiente bloque
        currentSeq = 1 - currentSeq;
	}	

	printf("There are no more bytes to read!\n");

	uint8_t endFrameBuffer[sizeof(Header)];
    size_t endFrameSize = buildEndFrame(currentSeq, endFrameBuffer);

    printf("Building END frame (size: %zu | Seq bit: %u)\n", endFrameSize, currentSeq);
    printf("Sending END frame...\n");
    sendFrameSockets(endFrameBuffer, endFrameSize);	// PENDIENTE: IMPLEMENTAR ESTA FUNCION

	closeInputFile(file);
	fclose(outputFile);
	return 0;
}