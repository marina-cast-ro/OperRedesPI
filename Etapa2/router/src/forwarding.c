#include "forwarding.h"

#include "protocol.h"
#include "routingTable.h"
#include "router.h"

#define MAX_NEIGHBORS 16  // Cuántos vecinos puede tener este router como máximo

// Falta implementar la función getNeighborSockets, que devuelve los sockets de los vecinos conectados.
//int getNeighborSockets(int *sockets, int maxSockets);
// Retorna la IP propia del router, la que viene en config.txt
//uint32_t getLocalIp(void);

// La tabla de rutas vive en una sola memoria simulada, y el hilo de escucha la escribe mientras alguien más la puede estar leyendo. Este candado evita que se corrompa.
static pthread_mutex_t tableMutex = PTHREAD_MUTEX_INITIALIZER;

// Manda el mensaje por un socket, agregándole el fin de línea que separa una trama de la otra
static void sendFrame(int socket, const char *message) {
    char frame[MAX_BUFFER_SIZE];
    size_t msgLen = strlen(message);
    int length;

    // Si ya trae salto de línea al final (como las tramas de DATA)
    if (msgLen > 0 && message[msgLen - 1] == '\n') {
        length = snprintf(frame, sizeof(frame), "%s", message);
    } else {
        length = snprintf(frame, sizeof(frame), "%s\n", message);
    }

    if (length > 0 && (size_t)length < sizeof(frame)) {
        send(socket, frame, (size_t)length, 0);
    }
}

// Arma un mensaje con una IP, y se lo manda a todos los vecinos menos por exceptSocket, que es por donde llegó. Con exceptSocket en -1 se lo manda a todos
static void announceToNeighbors(const char *type, uint32_t ip, int exceptSocket) {
    (void)exceptSocket; // Silencia el aviso de parámetro no usado
    char message[MAX_BUFFER_SIZE];
    char ipText[INET_ADDRSTRLEN];
    //int sockets[MAX_NEIGHBORS];
    struct in_addr address;

    address.s_addr = ip;
    inet_ntop(AF_INET, &address, ipText, sizeof(ipText));
    snprintf(message, sizeof(message), "%s%c%s", type, PROTOCOL_SEPARATOR, ipText);

    /* Lógica temporal desactivada */

    /*int count = getNeighborSockets(sockets, MAX_NEIGHBORS);
    for (int i = 0; i < count; i++) {
        if (sockets[i] != exceptSocket) {
            sendFrame(sockets[i], message);
        }
    }*/
}

// Aprende que a ip se llega por sockfd. Retorna 1 si la ruta era nueva, 0 si ya la conocíamos.
// La búsqueda y el guardado van dentro del mismo candado para que dos hilos no crean los dos que la ruta es nueva y la propaguen dos veces
static int learnRoute(uint32_t ip, int sockfd) {
    uint32_t knownSocket = 0;
    int isNew;

    pthread_mutex_lock(&tableMutex);
    isNew = (findRoute(ip, &knownSocket) != 0);
    if (isNew) {
        saveRoute(ip, (uint32_t)sockfd);
    }
    pthread_mutex_unlock(&tableMutex);

    return isNew;
}

void sendInitialAnnounce(void) {
    //announceToNeighbors(PROTOCOL_ANNOUNCE, getLocalIp(), -1);
	printf("Aca va sendInitialAnnounce() pero le faltan cosas");
}

void sendInitialAdvertise(void) {
    uint32_t hostIp;
    int index = 0;

    // Al arrancar, lo único que hay en la tabla son los hosts locales que precargó configParser
    while (1) {
        pthread_mutex_lock(&tableMutex);
        int found = getRouteIp(index, &hostIp);
        pthread_mutex_unlock(&tableMutex);

        if (found != 0) {
            return;
        }
        announceToNeighbors(PROTOCOL_ADVERTISE, hostIp, -1);
        index++;
    }
}

void processPacket(const char *buffer, int sockfd) {
    uint32_t destinationSocket = 0;
    RoutingMessage message;
    int found;

    if (decodeFrame(buffer, strlen(buffer), &message) != 0) {
        return;  // El mensaje no es válido, se descarta
    }

    switch (message.type) {
        // Un vecino se presenta, o propaga una IP que aprendió. Se guarda la ruta y, solo si no la conocíamos, se les avisa a los demás vecinos.
        // Si ya la conocíamos no se hace nada, y así el aviso no da vueltas para siempre
        case ROUTING_ANNOUNCEMENT:
        case ROUTING_ADVERTISEMENT:
            if (learnRoute(message.announcedIp, sockfd)) {
                announceToNeighbors(PROTOCOL_ADVERTISE, message.announcedIp, sockfd);
            }
            break;

        // Mensaje de datos: se busca en la tabla por cuál socket sale y se reenvía igual que como llegó.
        // Si el destino no está en la tabla, se descarta
        case ROUTING_DATA:
            pthread_mutex_lock(&tableMutex);
            found = findRoute(message.destinationIp, &destinationSocket);
            pthread_mutex_unlock(&tableMutex);

            if (found == 0) {
                sendFrame((int)destinationSocket, buffer);
            }
            break;
    }
}