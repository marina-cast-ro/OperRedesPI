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
    routerAddress->sin_port = htons(routerPort);
}

static void processMessage(RoutingMessage *msg){
    if(msg->type == ROUTING_DATA){
        char dataToShow[msg->dataLength + 1];
        memcpy(dataToShow, msg->data, msg->dataLength);
        dataToShow[msg->dataLength] = '\0'; //le agregamos caracter nulo al final
        showMessage(dataToShow);
    }
}

int keepListening(const char *routerIp, int routerPort){
    int actualSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (actualSocketFd < 0) {
        perror("socket");
        return -1;
    }
    //se establece informacion del socket
    struct sockaddr_in routerAddress;
    initRouterAddress(&routerAddress, routerPort);

    //verificamos si la direccion IP es valida. 
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

    printf("Se ha conectado al router exitosamente: %s:%d\n", routerIp, routerPort);

    int flag = 1;

    while(flag){
        char buffer[LISTENER_BUFFER_SIZE];
        RoutingMessage msg; 

        ssize_t receivedBytes = recv(actualSocketFd, buffer, sizeof(buffer) - 1, 0);
        if(receivedBytes > 0) buffer[receivedBytes] = '\0'; 

        if(receivedBytes <= 0){
            flag = 0; //si la conexion se cierra salimos del ciclo
        }
        else{
            if(decodeFrame(buffer, receivedBytes, &msg) == 0){
                processMessage(&msg);
            }
        }
    }
        close(actualSocketFd);
        return 0;
}

void showMessage(const char* receivedMessage){
    printf("[MENSAJE]: %s\n", receivedMessage);
}
