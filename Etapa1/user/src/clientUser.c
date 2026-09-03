#include "protocol.h"

#include <stdlib.h>
#include <sys/socket.h>

#define DEFAULT_OUTPUT_FILE "sensor_data.txt"

// Construye un Header de tipo ACK y lo envia de vuelta al mismo emisor que
// nos mando un frame.
//
// Parametros:
//   socketFd = socket UDP que ya estamos usando para recibir
//   senderAddress = struct con IP y puerto UDP del que nos mando el frame
//   senderLength = tamano del struct de senderAddress
//   nextExpectedSequence = numero de secuencia (0 o 1) que le vamos a decir al emisor que esperamos.
//
// Retorna bytes enviados o -1 en caso de rror

static int sendAck(int socketFd, const struct sockaddr_in *senderAddress
    ,socklen_t senderLength, uint8_t nextExpectedSequence) {

    Header ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = PROTOCOL_FRAME_ACK;
    ack.seqNumber = nextExpectedSequence;
    ack.payloadLength = htons(0);

    // Manda el mensaje de ACK por un socket
    return sendto(socketFd, &ack, sizeof(ack), 0
        ,(const struct sockaddr *)senderAddress, senderLength);
}


// Funcion del cliente receptor que hace todo el flujo
//
// Parametros:
//   localPort = puerto UDP en el que este cliente va a escuchar.
//   outputPath = ruta del archivo donde se van a guardar los datos.
//
int runClientReceiver(uint16_t localPort, const char *outputPath) {

    int clientSocket;
    struct sockaddr_in localAddress;
    FILE *outputFile;
    uint8_t expectedSequence = 0;

    if (!outputPath) {
        fprintf(stderr, "Error: archivo de salida invalido\n");
        return EXIT_FAILURE;
    }

    // Creamos el socket con IPv4, tipo de socket y su protocolo
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        perror("Error creando socket UDP");
        return EXIT_FAILURE;
    }

    // Establecemos informacion del socket
    memset(&localAddress, 0, sizeof(localAddress));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = htonl(INADDR_ANY); // acepta paquetes de cualquier interfaz de red
    localAddress.sin_port = htons(localPort);

    // Reservamos el puerto para este socket.
    if (bind(clientSocket, (struct sockaddr *)&localAddress, sizeof(localAddress)) < 0) {
        perror("Error al reservar el puerto para el socket.(bind)");
        close(clientSocket);
        return EXIT_FAILURE;
    }

    // Abrimos el archivo en modo append
    outputFile = fopen(outputPath, "a");
    if (!outputFile) {
        perror("Error abriendo archivo de salida");
        close(clientSocket);
        return EXIT_FAILURE;
    }

    printf("Cliente escuchando en puerto %u\n", localPort);
    printf("Guardando mediciones en %s\n", outputPath);

    // Bucle prinicpal
    for (;;) {
        Frame frame;
        struct sockaddr_in senderAddress;

        socklen_t senderLength = sizeof(senderAddress);
        ssize_t receivedBytes;
        uint16_t payloadLength;
        size_t frameLength;

        memset(&senderAddress, 0, sizeof(senderAddress));

        receivedBytes = recvfrom(clientSocket, &frame, sizeof(frame), 0
            , (struct sockaddr *)&senderAddress, &senderLength);
            
        if (receivedBytes < 0) {
            perror("Error al leer los bytes del socket.(recvfrom)");
            continue;
        }

        // Los frames deben tener minimo el Header (4 bytes). 
        // Si llego menos al frame esta corrupto o incompleto
        if ((size_t)receivedBytes < sizeof(Header)) {
            fprintf(stderr, "[Receptor] Frame ignorado: Incompleto o corrupto\n");
            continue;
        }

        payloadLength = ntohs(frame.header.payloadLength);
        frameLength = sizeof(Header) + payloadLength;

        // Verificacion de integridad de los datos:
            if (payloadLength > MAX_PAYLOAD_SIZE || frameLength != (size_t)receivedBytes) {
            fprintf(stderr, "[Receptor] Frame ignorado: longitud incorrecta\n");
            continue;
        }

        // Si el frame es de tipo END se envia el ACK y sigue escuchando
        if (frame.header.type == PROTOCOL_FRAME_END) {
			printf("[Receptor] Trama END recibida. Cerrando sesión lógica...\n");
			printf("--------------------------------------------------\n\n");
			expectedSequence = (uint8_t)(1 - expectedSequence);
            if (sendAck(clientSocket, &senderAddress, senderLength, expectedSequence) < 0) {
                perror("Error enviando ACK para END");
            }
            continue;
        }

        // Si el frame no es de tipo DATA se descarta por no tener informacion para guardar
        if (frame.header.type != PROTOCOL_FRAME_DATA) {
            fprintf(stderr, "[Receptor] Frame ignorado: No es tipo DATA\n");
            continue;
        }

        // Si el numero de secuencia coincide con lo que yo esperaba, 
        // guardo los datos y cambio el bit esperado.
        // Si no coincide, ya recibimos ese frame quiere decir que el emisor lo 
        // volvio a mandar (ACK se perdio) y no quiero guardar un dato repetido.
        if (frame.header.seqNumber == expectedSequence) {
			printf("[Receptor] Trama DATA válida | Seq: %u (Esperado) | Tamaño payload: %u bytes\n", 
                   frame.header.seqNumber, payloadLength);
            if (payloadLength > 0) {
                // Se escriben bytes en el archivo
                if (fwrite(frame.payload, 1, payloadLength, outputFile) != payloadLength) {
                    perror("Error escribiendo medicion en el archivo");
                    clearerr(outputFile);
                } else {
                    fflush(outputFile);
					printf("[Receptor] -> Datos guardados exitosamente en disco.\n");
                }
            }
            expectedSequence = (uint8_t)(1 - expectedSequence);
        } else {
            printf("[Receptor] Trama DUPLICADA detectada | Seq recibido: %u | Seq esperado: %u (Reenviando ACK previo)\n", 
                   frame.header.seqNumber, expectedSequence);
        }

        // Se manda ACK sin importar si los datos eran nuevo o duplicados
        if (sendAck(clientSocket, &senderAddress, senderLength, expectedSequence) < 0) {
            perror("Error enviando ACK");
        } else {
            printf("[Receptor] <- ACK enviado con próximo Seq esperado: %u\n", expectedSequence);
        }

		printf("--------------------------------------------------\n\n");
    }
    fclose(outputFile);
    close(clientSocket);
    return EXIT_SUCCESS;
}

// main de prueba
int main(int argc, char *argv[]) {
    uint16_t localPort = DEFAULT_PORT;
    const char *outputPath;

    if (argc > 2) {
        fprintf(stderr, "Uso: %s [archivo_salida]\n", argv[0]);
        return EXIT_FAILURE;
    }

    outputPath = (argc == 2) ? argv[1] : DEFAULT_OUTPUT_FILE;

    return runClientReceiver(localPort, outputPath);
}