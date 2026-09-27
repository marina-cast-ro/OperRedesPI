#include "sender.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SENDER_BUFFER_SIZE 1024

static void initRouterAddress(struct sockaddr_in *routerAddress, int routerPort){
    memset(routerAddress, 0, sizeof(*routerAddress)); //le pasamos como parametro la ip del router
    routerAddress->sin_family = AF_INET;
    routerAddress->sin_port = htons((uint16_t)routerPort);
}

static int buildFrame(const char *destIp, const char *message, RoutingMessage *msg) {
    if (!destIp || !message || !msg) {
        return -1;
    }

    struct in_addr address;
    
    // Convierte la dirección IP a formato binario de red (Big-Endian)
    if (inet_pton(AF_INET, destIp, &address) != 1) {
        fprintf(stderr, "Dirección IP inválida: %s\n", destIp);
        return -1;
    }

    msg->type = ROUTING_DATA;
    msg->announcedIp = 0; // Se limpia la IP de anuncio
    msg->destinationIp = address.s_addr;
    msg->data = message;  // Apunta a los datos (valido si message vive hasta llamar a encodeFrame)
    msg->dataLength = strlen(message);

    return 0;
}

int sendMessage(const char *routerIp, int routerPort, const char *destIp, const char *message){
    int actualSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (actualSocketFd < 0) {
        perror("[SENDER] socket");
        return -1;
    }
    //se establece informacion del socket
    struct sockaddr_in routerAddress;
    initRouterAddress(&routerAddress, routerPort);

    if(inet_pton(AF_INET, routerIp, &routerAddress.sin_addr) != 1){
        fprintf(stderr, "[SENDER] Dirección IP del router inválida: %s\n", routerIp);
        close(actualSocketFd);
        return -1;
    }

    //verificamos si el puerto es correcto
    if(connect(actualSocketFd, (struct sockaddr *)&routerAddress, sizeof(routerAddress))< 0){
        perror("[SENDER] connect");
        close(actualSocketFd);
        return -1;
    }

    RoutingMessage msg; //el mensaje que vamos a enviar
    
    if(buildFrame(destIp, message, &msg)){//cargamos en msg la informacion del mensaje
        fprintf(stderr, "[SENDER] Error en buildFrame con destIp: %s\n", destIp);
        close(actualSocketFd);
        return -1; //significa que el destIP es invalido
    } 

    char buffer[SENDER_BUFFER_SIZE];
    // Declara encodedBytes y guarda el retorno de encodeFrame
    int encodedBytes = encodeFrame(&msg, buffer, sizeof(buffer));
    if(encodedBytes <= 0){
        fprintf(stderr, "[SENDER] Error en encodeFrame\n");
        close(actualSocketFd);        
        return -1;
    }
    ssize_t dataSent = send(actualSocketFd, buffer, (size_t)encodedBytes, 0);
    if(dataSent < 0){
        perror("[SENDER] send");
        close(actualSocketFd);
        return -1;   
    }
    close(actualSocketFd);
    return 0;
}

int sendFile(const char *routerIp, int routerPort, const char *destIp, const char *filePath){
    if (!routerIp || !destIp || !filePath) {
        fprintf(stderr, "[SENDER] Parámetros inválidos para sendFile\n");
        return -1;
    }

    FILE *file = fopen(filePath, "r");
    if (!file) {
        perror("[SENDER] Error al abrir el archivo");
        return -1;
    }

    int actualSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (actualSocketFd < 0) {
        perror("[SENDER] socket");
        fclose(file);
        return -1;
    }

    struct sockaddr_in routerAddress;
    initRouterAddress(&routerAddress, routerPort);

    if (inet_pton(AF_INET, routerIp, &routerAddress.sin_addr) != 1) {
        fprintf(stderr, "[SENDER] Dirección IP del router inválida: %s\n", routerIp);
        close(actualSocketFd);
        fclose(file);
        return -1;
    }

    if (connect(actualSocketFd, (struct sockaddr *)&routerAddress, sizeof(routerAddress)) < 0) {
        perror("[SENDER] connect");
        close(actualSocketFd);
        fclose(file);
        return -1;
    }

    char line[900];
    int linesSent = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }

        RoutingMessage msg;
        if (buildFrame(destIp, line, &msg) != 0) {
            fprintf(stderr, "[SENDER] Error en buildFrame con destIp: %s\n", destIp);
            close(actualSocketFd);
            fclose(file);
            return -1;
        }

        char buffer[SENDER_BUFFER_SIZE];
        int encodedBytes = encodeFrame(&msg, buffer, sizeof(buffer));
        if (encodedBytes <= 0) {
            fprintf(stderr, "[SENDER] Error en encodeFrame\n");
            close(actualSocketFd);
            fclose(file);
            return -1;
        }

        ssize_t dataSent = send(actualSocketFd, buffer, (size_t)encodedBytes, 0);
        if (dataSent < 0) {
            perror("[SENDER] send");
            close(actualSocketFd);
            fclose(file);
            return -1;
        }

        linesSent++;
        usleep(2000);
    }

    if (linesSent == 0) {
        RoutingMessage msg;
        if (buildFrame(destIp, "", &msg) == 0) {
            char buffer[SENDER_BUFFER_SIZE];
            int encodedBytes = encodeFrame(&msg, buffer, sizeof(buffer));
            if (encodedBytes > 0) {
                send(actualSocketFd, buffer, (size_t)encodedBytes, 0);
            }
        }
    }

    fclose(file);
    close(actualSocketFd);
    printf("[SENDER] Archivo '%s' enviado exitosamente (%d líneas enviadas a %s)\n", filePath, linesSent, destIp);
    return 0;
}