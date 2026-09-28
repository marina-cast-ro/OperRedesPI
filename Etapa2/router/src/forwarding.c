#include "forwarding.h"

#include "protocol.h"
#include "routingTable.h"
#include "router.h"
#include "configParser.h"

#define MAX_NEIGHBORS 16

static pthread_mutex_t tableMutex = PTHREAD_MUTEX_INITIALIZER;

static void sendFrame(uint32_t targetIp, uint32_t nextHop, const char *message) {
    char frame[MAX_BUFFER_SIZE];
    size_t msgLen = strlen(message);
    int length;

    if (msgLen > 0 && message[msgLen - 1] == '\n') {
        length = snprintf(frame, sizeof(frame), "%s", message);
    } else {
        length = snprintf(frame, sizeof(frame), "%s\n", message);
    }
    if (length <= 0 || (size_t)length >= sizeof(frame)) return;

    if (isLocalHost(targetIp)) {
        // nuestro propio host: usar el socket persistente que ya tiene abierto
        int socket = (int)nextHop;
        if (socket <= 0) return;
        printf("[FORWARD-TCP] Enviando (persistente) al socket %d: %s", socket, frame);
        send(socket, frame, (size_t)length, MSG_NOSIGNAL);
    } else {
        // vecino router: conexión nueva por mensaje
        int port = getNeighborPortForIp(nextHop);
        if (port <= 0) return;
        printf("[FORWARD-TCP] Enviando (conexion nueva) a vecino, puerto %d: %s", port, frame);
        sendToIpPort(nextHop, port, frame, (size_t)length);
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

static void announceToNeighbors(const char *type, uint32_t ip, uint32_t exceptNextHop) {
    char message[MAX_BUFFER_SIZE];
    char ipText[INET_ADDRSTRLEN];
    struct in_addr address;

    address.s_addr = ip;
    inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
    snprintf(message, sizeof(message), "%s%c%s", type, PROTOCOL_SEPARATOR, ipText);

    int count = getNeighborCount();
    for (int i = 0; i < count; i++) {
        char neighborIpStr[16];
        int neighborPort = 0;
        if (getNeighborInfo(i, neighborIpStr, &neighborPort) != 0) continue;

        struct in_addr na;
        if (inet_pton(AF_INET, neighborIpStr, &na) != 1) continue;
        if (na.s_addr == exceptNextHop) continue;  // split horizon

        sendToIpPort(na.s_addr, neighborPort, message, strlen(message));
    }
}

static int learnRoute(uint32_t ip, int sockfd) {
    uint32_t knownNextHop = 0;
    int isNew = 0;

    if (sockfd <= 0 || ip == 0) return 0;

    uint32_t nextHop = isLocalHost(ip) ? (uint32_t)sockfd : getPeerIp(sockfd);
    if (nextHop == 0) return 0;   // no pudimos identificar al vecino, no aprendemos nada

    pthread_mutex_lock(&tableMutex);
    int found = (findRoute(ip, &knownNextHop) == 0);
    if (!found || knownNextHop != nextHop) {
        saveRoute(ip, nextHop);
        isNew = 1;
    }
    pthread_mutex_unlock(&tableMutex);

    return isNew;
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

static void sendTableToSocket(int announceSockfd) {
    int isNeighbor = (announceSockfd == getUdpSocket());

    uint32_t announcerIp = 0;
    int announcerPort = 0;

    if (isNeighbor) {
        announcerIp = getPeerIp(announceSockfd);
        announcerPort = getNeighborPortForIp(announcerIp);
        if (announcerPort <= 0) return;
    }

    uint32_t hostIp, routeNextHop;
    int index = 0;

    while (1) {
        pthread_mutex_lock(&tableMutex);
        int found = getRouteIp(index, &hostIp);
        if (found == 0) findRoute(hostIp, &routeNextHop);
        pthread_mutex_unlock(&tableMutex);
        if (found != 0) break;

        int skip = isNeighbor && (routeNextHop == announcerIp);  // split horizon solo aplica entre routers

        if (!skip) {
            char message[MAX_BUFFER_SIZE];
            char ipText[INET_ADDRSTRLEN];
            struct in_addr address;
            address.s_addr = hostIp;
            inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
            snprintf(message, sizeof(message), "%s%c%s", PROTOCOL_ADVERTISE, PROTOCOL_SEPARATOR, ipText);

            if (isNeighbor) {
                sendToIpPort(announcerIp, announcerPort, message, strlen(message));
            } else {
                char frame[MAX_BUFFER_SIZE];
                int len = snprintf(frame, sizeof(frame), "%s\n", message);
                if (len > 0 && (size_t)len < sizeof(frame)) {
                    send(announceSockfd, frame, (size_t)len, MSG_NOSIGNAL);
                }
            }
        }
        index++;
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

        announceToNeighbors(PROTOCOL_ADVERTISE, hostIp, 0);
        index++;
    }
}

void processPacket(const char *buffer, size_t length, int sockfd) {
    printf("[FORWARD] Paquete recibido en fd %d (%zu bytes): %s\n", sockfd, length, buffer);
    uint32_t destinationSocket = 0;
    RoutingMessage message;
    int found;

    if (decodeFrame(buffer, length, &message) != 0) return;

    // Ignorar anuncios sobre nuestra propia IP local si coincide
    if ((message.type == ROUTING_ANNOUNCEMENT || message.type == ROUTING_ADVERTISEMENT) &&
        message.announcedIp == getLocalIp()) {
        return;
    }

    switch (message.type) {
        case ROUTING_ANNOUNCEMENT:
             if (learnRoute(message.announcedIp, sockfd)) {
                announceToNeighbors(PROTOCOL_ADVERTISE, message.announcedIp, getPeerIp(sockfd));
            }
            sendTableToSocket(sockfd);
            break;
        

        case ROUTING_ADVERTISEMENT:
            // Proteger rutas de hosts locales de ser sobreescritas por rebotes de vecinos
            if (isLocalHost(message.announcedIp)) break;

            if (learnRoute(message.announcedIp, sockfd)) {
                announceToNeighbors(PROTOCOL_ADVERTISE, message.announcedIp, getPeerIp(sockfd));
            }
            break;

        case ROUTING_DATA:
            pthread_mutex_lock(&tableMutex);
            found = findRoute(message.destinationIp, &destinationSocket);
            pthread_mutex_unlock(&tableMutex);

            if (found == 0 && destinationSocket > 0) {
                sendFrame(message.destinationIp, destinationSocket, buffer);
            } else {
                printf("[FORWARD] No se encontró ruta. Paquete DATA descartado.\n");
            }
            break;
    }
}