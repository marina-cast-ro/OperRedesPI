#include "serverSender.h"
#include "serverFileReader.h"
#include "serverFrameBuilder.h"

int sendFileOverProtocol(const char *filePath, const char *ip, int port) {
    FILE *file = openInputFile(filePath);
    if (!file) return -1; //si no puede leer, reporta error

    uint8_t payloadBuffer[MAX_PAYLOAD_SIZE]; //tamanio maximo del archivo txt
    size_t payloadSize;
    int success = 1;

    while (success && (payloadSize = readNextPayload(file, payloadBuffer)) > 0) {
		uint8_t frameBuffer[sizeof(Header) + MAX_PAYLOAD_SIZE];
		size_t frameSize = buildFrame(PROTOCOL_FRAME_DATA, payloadBuffer, payloadSize, frameBuffer, sizeof(frameBuffer));//empaqueta el txt para enviarse
		
		// Probabilidad de 70% de éxito de envío
		int randomNum = (rand() % 100) + 1;
		if (randomNum < 70) {
			if (sendFrameSockets(ip, port, frameBuffer, frameSize) != 0) {
				printf("No se logro el guardado del frame\n");
				success = 0;
			}
		} else {
			printf("Paquete perdido (random = %d)\n", randomNum);
		}
    }

    uint8_t endFrameBuffer[sizeof(Header)];
    size_t endFrameSize = buildEndFrame(endFrameBuffer, sizeof(endFrameBuffer));
    
    if(sendFrameSockets(ip, port, endFrameBuffer, endFrameSize) != 0){
        success = 0;
    }

    closeInputFile(file);

    return success ? 0 : -1;
}