#include "forwarding.h"

#include "protocol.h"
#include "routingTable.h"
#include "router.h"
#include "configParser.h"

#define MAX_NEIGHBORS 16

static pthread_mutex_t tableMutex = PTHREAD_MUTEX_INITIALIZER;

// Envío TCP genérico a un socket ya abierto (Listener / PC local)
static void sendFrameTCP(int socket, const char *message) {
    if (socket <= 0) return;

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

// Envío TCP efímero a un router vecino por su IP
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
    uint32_t knownVia = 0;
    int isNew;
    int routeChanged = 0;

    // Si es ANNOUNCE por UDP, o si viene por TCP:
    // - Para nuestra PC local conectada por TCP guardamos su socket id (< 65536).
    // - Para routers lejanos guardamos la IP del vecino TCP que nos trajo el paquete (Next Hop).
    uint32_t via;
    if (isAnnounce && sockfd != getUdpSocket()) {
        via = (uint32_t)sockfd; // PC Local
    } else {
        via = getPeerIp(sockfd); // Router vecino (IP)
    }

    if (via == 0) return 0;

    pthread_mutex_lock(&tableMutex);
    
    isNew = (findRoute(ip, &knownVia) != 0);

    if (isNew || knownVia != via) {
        saveRoute(ip, via);
        routeChanged = 1;
    }

    pthread_mutex_unlock(&tableMutex);

    return routeChanged;
}

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
    uint32_t nextHop = 0;
    RoutingMessage message;
    int found;

    if (decodeFrame(buffer, length, &message) != 0) return;

    // Ignorar anuncios sobre nuestra propia IP local
    if ((message.type == ROUTING_ANNOUNCEMENT || message.type == ROUTING_ADVERTISEMENT) &&
        message.announcedIp == getLocalIp()) {
        return;
    }

    switch (message.type) {
        case ROUTING_ANNOUNCEMENT:
        case ROUTING_ADVERTISEMENT:
            if (learnRoute(message.announcedIp, sockfd, message.type == ROUTING_ANNOUNCEMENT)) {
                //announceToNeighborsTCP(PROTOCOL_ADVERTISE, message.announcedIp, sockfd);
            }
            break;

        case ROUTING_DATA:
            pthread_mutex_lock(&tableMutex);
            found = findRoute(message.destinationIp, &nextHop);
            pthread_mutex_unlock(&tableMutex);

            if (found == 0 && nextHop > 0) {
                if (nextHop < 65536) {  
                    // Descriptor de socket activo para nuestro Listener local
                    printf("[FORWARD] Ruta local encontrada. Reenviando por socket TCP %u...\n", nextHop);
                    sendFrameTCP((int)nextHop, buffer);
                } else {                  
                    // IP de un router vecino (Next Hop)
                    char ipText[INET_ADDRSTRLEN];
                    struct in_addr address;
                    address.s_addr = nextHop;
                    inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
                    
                    printf("[FORWARD] Ruta remota encontrada hacia %s. Reenviando por TCP...\n", ipText);
                    sendFrameIp(nextHop, buffer);
                }
            } else {
                printf("[FORWARD] No se encontró ruta. Paquete DATA descartado.\n");
            }
            break;
    }
}