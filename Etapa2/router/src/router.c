#include "../include/router.h"

//int socket_fd;

// Variables de estado del router
static int          socket_fd = -1;  // Socket para identificar una sesión activa
static pthread_t    listen_thread;   // Hilo de escucha del router
static volatile int running = 0;     // Estado (sensible) del router

static int peers_fds[MAX_PEERS];     // Lista de registro de los vecinos activos actuales

static void initPeers(void) {
    for (int i = 0; i < MAX_PEERS; i++) {
        peers_fds[i] = -1;
    }
}

int getNeighborSockets(int *sockets, int maxSockets) {
    if (sockets == NULL || maxSockets <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < MAX_PEERS && count < maxSockets; i++) {
        if (peers_fds[i] != -1) {
            sockets[count] = peers_fds[i];
            count++;
        }
    }

    return count;
}

// Función interna: El router registra un vecino entrante y estable
static void addPeer(int fd);

// Función interna: El router descarta un vecino que ya cumplió su función
static void removePeer(int fd);

// Llama por TCP a un router vecino. Retorna el socket conectado, o -1 si no contestó
static int connectToNeighbor(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1 || connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int initRouterListen(ConfigRouter router) {
    initPeers();

    // Creación del socket TCP del router para la escucha
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("[ROUTER] Error al crear el socket de escucha\n");
        return -1;
    }
    
    // Configuración base del socket TCP
    int opt = 1; 
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configuración de datos para el socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)router.port);

    // Bindeo del socket 
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(socket_fd);
        socket_fd = -1;
        return -1;
    }

    // Escucha del socket
    if (listen(socket_fd, 10) < 0) {
        perror("listen");
        close(socket_fd);
        socket_fd = -1;
        return -1;
    }

    // Llama a cada router vecino del config.txt. Si alguno todavía no está encendido no pasa nada: cuando arranque, él nos va a llamar a nosotros
    for (int i = 0; i < router.neighborCount; i++) {
        int fd = connectToNeighbor(router.neighborIp[i], router.neighborPort[i]);
        if (fd >= 0) {
            addPeer(fd);
            printf("[ROUTER] Conectado al vecino %s:%d por el socket %d\n",
                   router.neighborIp[i], router.neighborPort[i], fd);
        }
    }

    // Se presenta y cuenta qué PC viven con él a los vecinos que contestaron
    sendInitialAnnounce();
    sendInitialAdvertise();

    running = 1;

    // Creación del hilo de escucha
    if (pthread_create(&listen_thread, NULL, listening, NULL) != 0) {
        perror("pthread_create");
        close(socket_fd);
        socket_fd = -1;
        running = 0;
        return -1;
    }

    printf("[ROUTER] Inicialización exitosa del router con el puerto %d\n", router.port);
	return 0;
}

static void addPeer(int fd) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers_fds[i] == -1) {
            peers_fds[i] = fd;
            break;
        }
    }
}

static void removePeer(int fd) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers_fds[i] == fd) {
            peers_fds[i] = -1;
            break;
        }
    }
}

void *listening(void *arg) {
    (void)arg;
    char buffer[MAX_BUFFER_SIZE];

    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);            // Inicializa y vacía a los descriptores de archivo
        FD_SET(socket_fd, &read_fds);  // Añade el socket principal al conjunto de FD's
        
        // Añade los FD vecinos al conjunto FD
        int max_fd = socket_fd;
        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers_fds[i] != -1) {
                FD_SET(peers_fds[i], &read_fds);
                
                if (peers_fds[i] > max_fd) 
                    max_fd = peers_fds[i];
            }
        }

        // Timeout de 1s para revisar running constantemente
        struct timeval tv = {1, 0};
        // Vigila los FDs y avisa si hay alguno(s) con datos por leer
        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (ret < 0) {
            // Fallo en la syscall interna del select()
            if (errno == EINTR) 
                continue;
            // Fallo común en el select()
            perror("[ROUTER] Error en la función nativa select() para la vigilancia de descriptores de archivo (vecinos)");
            break;
        }
    
        // Timeout alcanzado
        if (ret == 0) continue;

        // Nueva conexion entrante
        if (FD_ISSET(socket_fd, &read_fds)) {
            struct sockaddr_in peer_address;
            socklen_t len = sizeof(peer_address);
            int new_FD = accept(socket_fd, (struct sockaddr *)&peer_address, &len);
            
            if (new_FD >= 0) {
                uint32_t ip_pc = peer_address.sin_addr.s_addr;
                
                // Guarda en la tabla de rutas la IP de origen asociada a este nuevo socket
                saveRoute(ip_pc, (uint32_t)new_FD);
                
                // Registra el socket en la lista de vecinos para select()
                addPeer(new_FD);

                // Al que acaba de llegar se le cuenta todo lo que sabemos, por si arrancó después
                sendInitialAnnounce();
                sendInitialAdvertise();
            }
        }

        // Datos entrantes en vecinos ya conectados
        for (int i = 0; i < MAX_PEERS; i++) {
            int fd = peers_fds[i];
            
            if (fd != -1 && FD_ISSET(fd, &read_fds)) {
                ssize_t n = recv(fd, buffer, MAX_BUFFER_SIZE - 1, 0);
                
                // Conexion cerrada por el vecino o un error
                if (n <= 0) {
                    printf("[ROUTER] Cliente/Listener desconectado en socket %d\n", fd);

                    removePeer(fd);
                    close(fd);
                }
                // Conexion establecida, se envía la trama para su procesamiento
                else {
                    buffer[n] = '\0';

                    // --- PROCESAMIENTO POR DELIMITADOR DE TRAMA (\n) ---
                    char *line_start = buffer;
                    char *line_end;

                    // Procesa cada trama individual separada por '\n' dentro del buffer leído
                    while ((line_end = strchr(line_start, '\n')) != NULL) {
                        *line_end = '\0'; // Corta la trama actual reemplazando el \n por \0
                        
                        size_t frame_len = (size_t)(line_end - line_start);
                        if (frame_len > 0) {
                            processPacket(line_start, frame_len, fd);
                        }

                        line_start = line_end + 1; // Avanza el puntero a la siguiente trama
                    }
                }
            }
        }
    }
 
    return NULL;
}

void endRouterListen(ConfigRouter router) {
    (void)router;
    
    running = 0;
    pthread_join(listen_thread, NULL);

    // Se cierran los descriptores de archivos de cada vecino
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers_fds[i] != -1) {
            close(peers_fds[i]);
            peers_fds[i] = -1;
        }
    }
 
    // Socket aún activo, procede a cerrarse y restablecer su valor
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }

    printf("[ROUTER] Finalización de escucha del router\n");
}
