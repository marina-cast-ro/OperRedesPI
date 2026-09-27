#include "listener.h"
#include "protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define LISTENER_BUFFER_SIZE 1024

static void initRouterAddress(struct sockaddr_in *routerAddress, int routerPort){
    memset(routerAddress, 0, sizeof(*routerAddress)); //le pasamos como parametro la ip del router
    routerAddress->sin_family = AF_INET;
    routerAddress->sin_port = htons((uint16_t)routerPort);
}

static const char *g_outputFile = "output.txt";

void saveMessageToFile(const char *filePath, const char *receivedMessage){
    if (!filePath || !receivedMessage) return;
    FILE *file = fopen(filePath, "a");
    if (!file) {
        perror("[HOST LISTENER] Error al escribir en el archivo");
        return;
    }
    fprintf(file, "%s\n", receivedMessage);
    fclose(file);
}

void showMessage(const char* receivedMessage){
    printf("[MENSAJE]: %s\n", receivedMessage);
}

static void processMessage(RoutingMessage *msg){
    if(msg->type == ROUTING_DATA && msg->data != NULL){
        char dataToShow[msg->dataLength + 1];
        memcpy(dataToShow, msg->data, msg->dataLength);
        dataToShow[msg->dataLength] = '\0'; //le agregamos caracter nulo al final
        showMessage(dataToShow);
        saveMessageToFile(g_outputFile, dataToShow);
    }
}

int keepListening(const char *routerIp, int routerPort, const char *hostLogicalIp, const char *outputFilePath){
    if (outputFilePath && strlen(outputFilePath) > 0) {
        g_outputFile = outputFilePath;
    } else {
        g_outputFile = "output.txt";
    }

    FILE *initFile = fopen(g_outputFile, "w");
    if (initFile) {
        fclose(initFile);
        printf("[HOST LISTENER] Guardando datos recibidos en '%s'\n", g_outputFile);
    } else {
        perror("[HOST LISTENER] Advertencia: no se pudo inicializar archivo de salida");
    }

    int actualSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (actualSocketFd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in routerAddress;
    initRouterAddress(&routerAddress, routerPort);

    if(inet_pton(AF_INET, routerIp, &routerAddress.sin_addr) != 1){
        fprintf(stderr, "Dirección IP inválida: %s\n", routerIp);
        close(actualSocketFd);
        return -1;
    }

    if(connect(actualSocketFd, (struct sockaddr *)&routerAddress, sizeof(routerAddress)) < 0){
        perror("connect");
        close(actualSocketFd);
        return -1;
    }

    // --- REGISTRAR LA IP LÓGICA ---
    char announceFrame[64];
    snprintf(announceFrame, sizeof(announceFrame), "%s%c%s\n", PROTOCOL_ANNOUNCE, PROTOCOL_SEPARATOR, hostLogicalIp);
    send(actualSocketFd, announceFrame, strlen(announceFrame), 0);

    printf("Se ha conectado al router exitosamente: %s:%d\n", routerIp, routerPort);

    int flag = 1;

    while(flag){
        char buffer[LISTENER_BUFFER_SIZE];

        // Única lectura por iteración usando actualSocketFd
        ssize_t receivedBytes = recv(actualSocketFd, buffer, sizeof(buffer) - 1, 0);

        if(receivedBytes <= 0){
            printf("[HOST LISTENER] Conexión cerrada por el router.\n");
            flag = 0; 
        }
        else {
            buffer[receivedBytes] = '\0';

            char *line_start = buffer;
            char *line_end;

            while ((line_end = strchr(line_start, '\n')) != NULL) {
                *line_end = '\0';
                size_t frame_len = (size_t)(line_end - line_start);

                if (frame_len > 0) {
                    RoutingMessage msg;
                    if (decodeFrame(line_start, frame_len, &msg) == 0) {
                        if (msg.type == ROUTING_DATA) {
                            printf("[HOST LISTENER] Bytes recibidos (%zd bytes): [%s]\n", frame_len, line_start);
                            processMessage(&msg);
                        }
                    }
                }
                line_start = line_end + 1;
            }
        }
    }
    
    close(actualSocketFd);
    return 0;
}
