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
    routerAddress->sin_port = htons(routerPort);
}

static int buildFrame(const char *destIp, const char *message, RoutingMessage *msg){
    struct in_addr address;
    
    //se convierte la direccion IP a formato binario
    //devuelve 1 si se hizo la conversion 
    if(inet_pton(AF_INET, destIp, &address) != 1){
        fprintf(stderr, "Dirección IP inválida: %s\n", destIp);
        return -1;
    }

    //se arma una trama
    msg->type = ROUTING_DATA;
    msg->destinationIp = address.s_addr;
    msg->data = message;
    msg->dataLength = strlen(message);

    return 0;
}

int sendMessage(const char *routerIp, int routerPort, const char *destIp, const char *message){
    int actualSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (actualSocketFd < 0) {
        perror("socket");
        return -1;
    }
    //se establece informacion del socket
    struct sockaddr_in routerAddress;
    initRouterAddress(&routerAddress, routerPort);

    if(inet_pton(AF_INET, routerIp, &routerAddress.sin_addr) != 1){
        fprintf(stderr, "Dirección IP inválida: %s\n", routerIp);
        close(actualSocketFd);
        return -1;
    }

    //verificamos si el puerto es correcto
    if(connect(actualSocketFd, (struct sockaddr *)&routerAddress, sizeof(routerAddress))< 0){
        perror("connect");
        close(actualSocketFd);
        return -1;
    }

    RoutingMessage msg; //el mensaje que vamos a enviar
    
    if(buildFrame(destIp, message, &msg)){//cargamos en msg la informacion del mensaje
        close(actualSocketFd);
        return -1; //significa que el destIP es invalido
    } 

    char buffer[SENDER_BUFFER_SIZE];
    // Declara encodedBytes y guarda el retorno de encodeFrame
    int encodedBytes = encodeFrame(&msg, buffer, sizeof(buffer));
    if(encodedBytes <= 0){
        close(actualSocketFd);        
        return -1;
    }
    ssize_t dataSent = send(actualSocketFd, buffer, encodedBytes, 0);
    if(dataSent < 0){
        perror("send");
        close(actualSocketFd);
        return -1;   
    }
    close(actualSocketFd);
    return 0;
}