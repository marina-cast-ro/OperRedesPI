#include "../include/router.h"

static int          tcp_socket_fd = -1;
static int          udp_socket_fd = -1;
static pthread_t    listen_thread;
static volatile int running = 0;

static ConfigRouter g_routerConfig; // Copia global de la configuración para los getters

static int peers_fds[MAX_PEERS];
static uint32_t peer_ips[1024];  // IP real de quien está del otro lado de cada socket
static pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

static void initPeers(void) {
    pthread_mutex_lock(&peers_mutex);
    for (int i = 0; i < MAX_PEERS; i++) {
        peers_fds[i] = -1;
    }
    pthread_mutex_unlock(&peers_mutex);
}

int getNeighborSockets(int *sockets, int maxSockets) {
    if (sockets == NULL || maxSockets <= 0) return 0;

    int count = 0;
    pthread_mutex_lock(&peers_mutex);
    for (int i = 0; i < MAX_PEERS && count < maxSockets; i++) {
        if (peers_fds[i] != -1) {
            sockets[count] = peers_fds[i];
            count++;
        }
    }
    pthread_mutex_unlock(&peers_mutex);

    return count;
}

static void addPeer(int fd) {
    pthread_mutex_lock(&peers_mutex);
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers_fds[i] == -1) {
            peers_fds[i] = fd;
            break;
        }
    }
    pthread_mutex_unlock(&peers_mutex);
}

static void removePeer(int fd) {
    pthread_mutex_lock(&peers_mutex);
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers_fds[i] == fd) {
            peers_fds[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&peers_mutex);
}

static int connectToNeighborTCP(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (fd < 1024) peer_ips[fd] = addr.sin_addr.s_addr;
    return fd;
}

uint32_t getPeerIp(int fd) {
    return (fd >= 0 && fd < 1024) ? peer_ips[fd] : 0;
}

int sendToIp(uint32_t ip, const char *data, size_t length) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct timeval timeout = {1, 0};  // Si no contesta, no quedarse pegado
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)g_routerConfig.port);
    addr.sin_addr.s_addr = ip;

    int sent = connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
               send(fd, data, length, MSG_NOSIGNAL) >= 0;
    close(fd);
    return sent ? 0 : -1;
}

// --- Getters requeridos por forwarding.c ---

int getUdpSocket(void) {
    return udp_socket_fd;
}

int getNeighborCount(void) {
    return g_routerConfig.neighborCount;
}

int getNeighborInfo(int index, char *outIp, int *outPort) {
    if (index < 0 || index >= g_routerConfig.neighborCount) return -1;
    if (outIp) strcpy(outIp, g_routerConfig.neighborIp[index]);
    if (outPort) *outPort = g_routerConfig.neighborPort[index];
    return 0;
}

// --- Inicialización y Escucha ---

int initRouterListen(ConfigRouter router) {
    g_routerConfig = router; // Guardamos la configuración para los getters
    initPeers();

    int opt = 1;

    // 1. Inicializar Socket UDP (exclusivo para ANNOUNCE)
    udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_fd < 0) {
        perror("[ROUTER] Error creando socket UDP");
        return -1;
    }
    setsockopt(udp_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons((uint16_t)router.port);

    if (bind(udp_socket_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("[ROUTER] Error en bind UDP");
        close(udp_socket_fd);
        return -1;
    }

    // 2. Inicializar Socket TCP (para ADVERTISE y DATA)
    tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket_fd < 0) {
        perror("[ROUTER] Error creando socket TCP");
        close(udp_socket_fd);
        return -1;
    }
    setsockopt(tcp_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons((uint16_t)router.port);

    if (bind(tcp_socket_fd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("[ROUTER] Error en bind TCP");
        close(udp_socket_fd);
        close(tcp_socket_fd);
        return -1;
    }

    if (listen(tcp_socket_fd, 10) < 0) {
        perror("[ROUTER] Error en listen TCP");
        close(udp_socket_fd);
        close(tcp_socket_fd);
        return -1;
    }

    // 3. Arrancar Hilo Listener Único
    running = 1;
    if (pthread_create(&listen_thread, NULL, listening, (void *)&router) != 0) {
        perror("[ROUTER] Error creando hilo listener");
        close(udp_socket_fd);
        close(tcp_socket_fd);
        running = 0;
        return -1;
    }

    // 4. Conectar TCP con vecinos
    for (int i = 0; i < router.neighborCount; i++) {
        int fd = connectToNeighborTCP(router.neighborIp[i], router.neighborPort[i]);
        if (fd >= 0) {
            addPeer(fd);
            printf("[ROUTER] Conectado TCP al vecino %s:%d por el socket %d\n",
                   router.neighborIp[i], router.neighborPort[i], fd);
        }
    }

    // 5. Envío Inicial
    sendInitialAnnounce();
    sendInitialAdvertise();

    printf("[ROUTER] Inicialización exitosa (UDP+TCP) en puerto %d\n", router.port);
    return 0;
}

void *listening(void *arg) {
    (void)arg;
    char buffer[MAX_BUFFER_SIZE];

    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        FD_SET(udp_socket_fd, &read_fds);
        FD_SET(tcp_socket_fd, &read_fds);

        int max_fd = (udp_socket_fd > tcp_socket_fd) ? udp_socket_fd : tcp_socket_fd;

        pthread_mutex_lock(&peers_mutex);
        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers_fds[i] != -1) {
                FD_SET(peers_fds[i], &read_fds);
                if (peers_fds[i] > max_fd) max_fd = peers_fds[i];
            }
        }
        pthread_mutex_unlock(&peers_mutex);

        struct timeval tv = {1, 0};
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[ROUTER] Error en select()");
            break;
        }

        if (ret == 0) continue;

        // A. Recepción Datagrama UDP (ANNOUNCE)
        if (FD_ISSET(udp_socket_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            ssize_t n = recvfrom(udp_socket_fd, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr *)&client_addr, &addr_len);
            if (n > 0) {
                buffer[n] = '\0';
                if (udp_socket_fd < 1024) peer_ips[udp_socket_fd] = client_addr.sin_addr.s_addr;
                processPacket(buffer, (size_t)n, udp_socket_fd);
            }
        }

        // B. Nueva Conexión TCP Entrante
        if (FD_ISSET(tcp_socket_fd, &read_fds)) {
            struct sockaddr_in peer_address;
            socklen_t len = sizeof(peer_address);
            int new_FD = accept(tcp_socket_fd, (struct sockaddr *)&peer_address, &len);

            if (new_FD >= 0) {
                if (new_FD < 1024) peer_ips[new_FD] = peer_address.sin_addr.s_addr;
                addPeer(new_FD);
            }
        }

        // C. Lectura Datos TCP Entrantes (ADVERTISE / DATA)
        int active_sockets[MAX_PEERS];
        int active_count = getNeighborSockets(active_sockets, MAX_PEERS);

        for (int i = 0; i < active_count; i++) {
            int fd = active_sockets[i];

            if (fd != -1 && FD_ISSET(fd, &read_fds)) {
                ssize_t n = recv(fd, buffer, MAX_BUFFER_SIZE - 1, 0);

                if (n <= 0) {
                    printf("[ROUTER] Cliente TCP desconectado en socket %d\n", fd);
                    removePeer(fd);
                    close(fd);
                } else {
                    buffer[n] = '\0';
                    int hasNewline = (strchr(buffer, '\n') != NULL);  // Antes de que el ciclo los borre

                    char *line_start = buffer;
                    char *line_end;

                    while ((line_end = strchr(line_start, '\n')) != NULL) {
                        *line_end = '\0';

                        if (line_end > line_start && *(line_end - 1) == '\r') {
                            *(line_end - 1) = '\0';
                        }

                        size_t frame_len = strlen(line_start);
                        if (frame_len > 0) {
                            processPacket(line_start, frame_len, fd);
                        }

                        line_start = line_end + 1;
                    }

                    if (!hasNewline) {  // Mensaje sin salto de línea, como los de otros grupos
                        processPacket(buffer, (size_t)n, fd);
                    }
                }
            }
        }
    } // Fin del while (running)

    return NULL;
} // Fin de listening()

void endRouterListen(ConfigRouter router) {
    (void)router;

    running = 0;
    pthread_join(listen_thread, NULL);

    pthread_mutex_lock(&peers_mutex);
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers_fds[i] != -1) {
            close(peers_fds[i]);
            peers_fds[i] = -1;
        }
    }
    pthread_mutex_unlock(&peers_mutex);

    if (tcp_socket_fd != -1) close(tcp_socket_fd);
    if (udp_socket_fd != -1) close(udp_socket_fd);

    printf("[ROUTER] Finalización de escucha del router\n");
}