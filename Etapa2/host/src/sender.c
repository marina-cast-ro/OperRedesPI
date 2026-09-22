#include "sender.h"
#include "serverFrameBuilder.h"
#include <string.h>
#include <stdio.h>

// TODO: aun no existe un tipo HELLO en protocol.h, se usa DATA como placeholder
// TODO: el Header no tiene campo de IP destino; esto depende del protocolo
//       que se defina en clase (inter-dominio). Preguntar al grupo.
int greetAndMeet(const char *routerIp, int routerPort, const char *ownIp) {
    uint8_t frame[MAX_BUFFER_SIZE];
    size_t frameSize = buildFrame(PROTOCOL_FRAME_DATA, (const uint8_t *)ownIp, strlen(ownIp), frame, sizeof(frame));
    if (frameSize == 0) {
        fprintf(stderr, "Error armando el anuncio de IP\n");
        return -1;
    }
    return sendFrameSockets(routerIp, routerPort, frame, frameSize);
}

int sendMessage(const char *routerIP, int routerPort, const char *destIP, int destPort, const char *message) {
    // TODO: destIp/destPort no caben en el Header actual (4 bytes).
    // Por ahora se manda solo el mensaje; falta cómo indicar el destino final.
    uint8_t frame[MAX_BUFFER_SIZE];
    size_t frameSize = buildFrame(PROTOCOL_FRAME_DATA, (const uint8_t *)message, strlen(message), frame, sizeof(frame));
    if (frameSize == 0) {
        fprintf(stderr, "Error armando el mensaje\n");
        return -1;
    }
    return sendFrameSockets(routerIP, routerPort, frame, frameSize);
}