
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

	while((payloadSize = readNextPayload(file, payloadBuffer)) > 0) {
		fwrite(payloadBuffer, 1, payloadSize, outputFile);
			
		printf("Read bytes from file: %zu\n", payloadSize);

		uint8_t frameBuffer[sizeof(Header) + MAX_PAYLOAD_SIZE];

		// Se construye la trama
		size_t frameSize = buildFrame(PROTOCOL_FRAME_DATA, payloadBuffer, payloadSize, frameBuffer, sizeof(frameBuffer));
		printf("Frame size with header: %zu\n", frameSize);

		printf("Sending frame to kernel space via syscall...\n");

		// Bloquea hasta que la syscall garantice la entrega confiable en Kernel
        int status = sendFrameSockets(frameBuffer, frameSize);
        
        if (status == 0) {
            printf("Frame delivered successfully!\n");
			printf("\n-----------------------------\n\n");
        } else {
            // Manejo de error suave: loggear y/o reintentar el mismo bloque
            fprintf(stderr, "Warning: Retrying block transmission...\n");
            // Aquí se podría hacer un reintento si la syscall falló definitivamente
        }
	}	

	printf("File read complete. Sending END frame...\n");

    uint8_t endFrameBuffer[sizeof(Header)];
    size_t endFrameSize = buildEndFrame(endFrameBuffer, sizeof(endFrameBuffer));

    sendFrameSockets(endFrameBuffer, endFrameSize);
    printf("Transmission finished.\n");


	closeInputFile(file);
	fclose(outputFile);
	return 0;
}