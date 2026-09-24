#include "listener.h"
#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

//Parte de las funciones de este archivo se obtuvo de clientUser.c
static int sendAck(int actualSocketFd, const struct sockaddr_in *senderAddress
    ,socklen_t senderLength, uint8_t nextExpectedSequence) {

    Header ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = PROTOCOL_FRAME_ACK;
    ack.seqNumber = nextExpectedSequence;
    ack.payloadLength = htons(0);

    // Manda el mensaje de ACK por un socket
    return sendto(actualSocketFd, &ack, sizeof(ack), 0
        ,(const struct sockaddr *)senderAddress, senderLength);
}

int keepListening(int localPort){
    int actualSocketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (actualSocketFd < 0) {
        perror("socket");
        return -1;
    }
            //se establece informacion del socket
        struct sockaddr_in localAddress;
        memset(&localAddress, 0, sizeof(localAddress));
        localAddress.sin_family = AF_INET;
        localAddress.sin_addr.s_addr = htonl(INADDR_ANY); // acepta paquetes de cualquier interfaz de red
        localAddress.sin_port = htons(localPort);

        // Reservamos el puerto para este socket.
        if (bind(actualSocketFd, (struct sockaddr *)&localAddress, sizeof(localAddress)) < 0) {
            perror("Error al reservar el puerto para el socket.(bind)");
            close(actualSocketFd);
            return -1;
        }

        printf("Escuchando en puerto %d\n", localPort);
        uint8_t expectedSequence = 1;

        while(1){
        Frame frame;
        struct sockaddr_in senderAddress;
        socklen_t senderLength = sizeof(senderAddress);

        ssize_t receivedBytes = recvfrom(actualSocketFd, &frame, sizeof(frame), 0, (struct sockaddr *)&senderAddress, &senderLength);
        if (receivedBytes < (ssize_t)sizeof(Header)) continue;

        uint16_t payloadLength = ntohs(frame.header.payloadLength);
        if (payloadLength > MAX_PAYLOAD_SIZE || sizeof(Header) + payloadLength != (size_t)receivedBytes) continue;

        if (frame.header.type == PROTOCOL_FRAME_END) {
            expectedSequence = 1 - expectedSequence;
            sendAck(actualSocketFd, &senderAddress, senderLength, expectedSequence);
            continue;
        }

        if (frame.header.type != PROTOCOL_FRAME_DATA) continue;

        if (frame.header.seqNumber == expectedSequence) {
            printf("[Mensaje] %.*s\n", payloadLength, frame.payload);
            expectedSequence = 1 - expectedSequence;
        }

        sendAck(actualSocketFd, &senderAddress, senderLength, expectedSequence);
        }
            close(actualSocketFd);
            return 0;
}
