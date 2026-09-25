#include "forwarding.h"

#include "protocol.h"
#include "routingTable.h"
#include "router.h"
#include "configParser.h"

#define MAX_NEIGHBORS 16

static pthread_mutex_t tableMutex = PTHREAD_MUTEX_INITIALIZER;

// Envío TCP genérico
static void sendFrameTCP(int socket, const char *message) {
    char frame[MAX_BUFFER_SIZE];
    size_t msgLen = strlen(message);
    int length;

    if (msgLen > 0 && message[msgLen - 1] == '\n') {
        length = snprintf(frame, sizeof(frame), "%s", message);
    } else {
        length = snprintf(frame, sizeof(frame), "%s\n", message);
    }

    if (length > 0 && (size_t)length < sizeof(frame)) {
        printf("[FORWARD-TCP] Enviando trama limpia al socket %d: %s", socket, frame);
        send(socket, frame, (size_t)length, MSG_NOSIGNAL);
    }
}

// Envío UDP exclusivo para ANNOUNCE
static void sendAnnounceUDP(uint32_t targetIp, int port, const char *message) {
    int sock = getUdpSocket();
    if (sock < 0) return;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons((uint16_t)port);
    dest.sin_addr.s_addr = targetIp;

    char frame[MAX_BUFFER_SIZE];
    snprintf(frame, sizeof(frame), "%s\n", message);

    sendto(sock, frame, strlen(frame), 0, (struct sockaddr *)&dest, sizeof(dest));
}

// Envío a un router: conexión nueva por mensaje, como hacen los otros grupos
static void sendFrameIp(uint32_t ip, const char *message) {
    char frame[MAX_BUFFER_SIZE];
    char ipText[INET_ADDRSTRLEN];
    struct in_addr address;
    size_t msgLen = strlen(message);
    int length;

    if (msgLen > 0 && message[msgLen - 1] == '\n') {
        length = snprintf(frame, sizeof(frame), "%s", message);
    } else {
        length = snprintf(frame, sizeof(frame), "%s\n", message);
    }

    if (length > 0 && (size_t)length < sizeof(frame)) {
        address.s_addr = ip;
        inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
        printf("[FORWARD-TCP] Enviando a %s: %s", ipText, frame);
        sendToIp(ip, frame, (size_t)length);
    }
}

static void announceToNeighborsTCP(const char *type, uint32_t ip, int exceptSocket) {
    char message[MAX_BUFFER_SIZE];
    char ipText[INET_ADDRSTRLEN];
    struct in_addr address;

    address.s_addr = ip;
    inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
    snprintf(message, sizeof(message), "%s%c%s", type, PROTOCOL_SEPARATOR, ipText);

    // A cada vecino del config por su IP, menos al que nos lo contó
    uint32_t exceptIp = getPeerIp(exceptSocket);
    int count = getNeighborCount();
    for (int i = 0; i < count; i++) {
        char neighborIp[16];
        struct in_addr neighbor;
        if (getNeighborInfo(i, neighborIp, NULL) == 0 &&
            inet_pton(AF_INET, neighborIp, &neighbor) == 1 && neighbor.s_addr != exceptIp) {
            sendFrameIp(neighbor.s_addr, message);
        }
    }
}

static int learnRoute(uint32_t ip, int sockfd, int isAnnounce) {
    uint32_t known = 0;
    int isNew;

    // Nuestra PC se presenta por su conexión TCP, que queda abierta: se guarda el socket.
    // Todo lo demás viene de un router: se guarda su IP, porque los otros grupos cierran
    // la conexión después de cada mensaje y el socket deja de servir
    uint32_t via = (isAnnounce && sockfd != getUdpSocket()) ? (uint32_t)sockfd : getPeerIp(sockfd);

    pthread_mutex_lock(&tableMutex);
    
    isNew = (findRoute(ip, &known) != 0);
    if (isNew || isAnnounce) {
        saveRoute(ip, via);
    }
    pthread_mutex_unlock(&tableMutex);

    // Solo se propaga lo nuevo, así los avisos no dan vueltas entre routers
    return isNew;
}

// ANNOUNCE: Transmitido por UDP usando la lista de vecinos precargada
void sendInitialAnnounce(void) {
    char message[MAX_BUFFER_SIZE];
    char ipText[INET_ADDRSTRLEN];
    struct in_addr address;

    address.s_addr = getLocalIp();
    inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
    snprintf(message, sizeof(message), "%s%c%s", PROTOCOL_ANNOUNCE, PROTOCOL_SEPARATOR, ipText);

    int count = getNeighborCount();
    for (int i = 0; i < count; i++) {
        char neighborIp[16];
        int neighborPort = 0;
        
        if (getNeighborInfo(i, neighborIp, &neighborPort) == 0) {
            struct in_addr destAddr;
            if (inet_pton(AF_INET, neighborIp, &destAddr) == 1) {
                sendAnnounceUDP(destAddr.s_addr, neighborPort, message);
            }
        }
    }
}

// ADVERTISE: Se envía por TCP a los vecinos conectados
void sendInitialAdvertise(void) {
    uint32_t hostIp;
    int index = 0;

    while (1) {
        pthread_mutex_lock(&tableMutex);
        int found = getRouteIp(index, &hostIp);
        pthread_mutex_unlock(&tableMutex);

        if (found != 0) break;

        announceToNeighborsTCP(PROTOCOL_ADVERTISE, hostIp, -1);
        index++;
    }
}

void processPacket(const char *buffer, size_t length, int sockfd) {
    printf("[FORWARD] Paquete recibido en fd %d (%zu bytes): %s\n", sockfd, length, buffer);
    uint32_t destinationSocket = 0;
    RoutingMessage message;
    int found;

    if (decodeFrame(buffer, length, &message) != 0) return;

    switch (message.type) {
        case ROUTING_ANNOUNCEMENT:
        case ROUTING_ADVERTISEMENT:
            if (learnRoute(message.announcedIp, sockfd, message.type == ROUTING_ANNOUNCEMENT)) {
                announceToNeighborsTCP(PROTOCOL_ADVERTISE, message.announcedIp, sockfd);
            }
            break;

        case ROUTING_DATA:
            pthread_mutex_lock(&tableMutex);
            found = findRoute(message.destinationIp, &destinationSocket);
            pthread_mutex_unlock(&tableMutex);

            if (found == 0) {
                if (destinationSocket < 65536) {  // Un número chico es el socket abierto de nuestra PC
                    printf("[FORWARD] Ruta encontrada. Reenviando por socket TCP %u...\n", destinationSocket);
                    sendFrameTCP((int)destinationSocket, buffer);
                } else {                          // Uno grande es la IP de un router
                    sendFrameIp(destinationSocket, buffer);
                }
            } else {
                printf("[FORWARD] No se encontró ruta. Paquete descartado.\n");
            }
            break;
    }
}