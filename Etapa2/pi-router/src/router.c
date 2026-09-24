#include "../include/router.h"

//int socket_fd;

int initRouterListen(ConfigRouter router) {
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
    addr.sin_addr.s_addr = router.localIp;
    addr.sin_port = htons(router.port);
 
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
 
    running = 1;
 
    // Creación del hilo de escucha
    if (pthread_create(&listen_thread, NULL, listening, NULL) != 0) {
        perror("pthread_create");
        close(socket_fd);
        socket_fd = -1;
        running = 0;
        return -1;
    }
 
    print("[ROUTER] Inicialización exitosa del router con el puerto %d\n", router.port);
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
            perror("[ROUTER] Error en la función nativa select() para la vigilancia de 
                descriptores de archivo (vecinos)");
            break;
        }
    
        // Timeout alcanzado
        if (ret == 0) continue;
 
        // Nueva conexion entrante
        if (FD_ISSET(socket_fd, &read_fds)) {
            // Configuración de los datos para aceptar al FD
            struct sockaddr_in peer_address;
            socklen_t len = sizeof(peer_address);
            // Aceptación del nodo vecino entrante
            int new_FD = accept(socket_fd, (struct sockaddr *)&peer_address, &len);
            
            // Aceptación exitosa
            // Se envía la info a la MMU + Guardado del nuevo vecino
            if (new_FD >= 0) {
                uint32_t ip_pc = peer_address.sin_addr.s_addr;
                saveRoute(ip_pc, new_FD);
                addpeerFd(new_FD);
            }
        }
 
        // Datos entrantes en vecinos ya conectados
        for (int i = 0; i < MAX_PEERS; i++) {
            int fd = peers_fds[i];
            
            if (fd != -1 && FD_ISSET(fd, &read_fds)) {
                ssize_t n = recv(fd, buffer, MAX_BUFFER_SIZE - 1, 0);
                
                // Conexion cerrada por el vecino o un error
                if (n <= 0) {
                    removepeerFd(fd);
                    close(fd);
                } 
                // Conexion establecida, se envía la trama para su procesamiento
                else {
                    buffer[n] = '\0';
                    processPacket(buffer, fd);
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



/**ConfigRouter initRouter(void) {
    ConfigRouter router;
    router.num_peers = 0;
    
    // Lectura del archivo de configuración del router
    FILE *archivo = fopen("../config.txt", "r");

    if (!archivo) {
        printf("[ROUTER] Error al abrir el archivo de configuración <config.txt>");
        exit(1);
    }

    char linea[256];

    while (fgets(linea, sizeof(linea), archivo)) {
        // 1. Identificador del router propio
        if (strncmp(linea, "router_id:", 10) == 0) {
            sscanf(linea, "router_id: %s", router.id);
        } 
        // 2. Puerto del router propio
        else if (strncmp(linea, "puerto:", 7) == 0) {
            sscanf(linea, "puerto: %d", &router.port);
        } 
        // 3. Dirección IP del router
        else if (strncmp(linea, "ip:", 3) == 0) {
            sscanf(linea, "ip: %s", router.ip);
        }
        // 4. Vecinos directos del router
        else if (strncmp(linea, "vecino:", 7) == 0) {
            Peer *p = &router.peers[router.num_peers];
            sscanf(linea, "vecino: %s %s %d", p->id, p->ip, &p->port);
            router.num_peers++;
        }
    }

    // Revisión de los datos guardados en el Router
    printf("================ DATOS DEL ROUTER ================\n");
    printf("Router ID: %s | Router IP: %s | Router Port: %d\n", router.id, router.ip, router.port);
    printf("Cantidad de vecinos encontrados: %d\n", router.num_peers);
    
    for (int i = 0; i < router.num_peers; i++) {
        printf("Vecino #%d: ID = %s | IP = %s | Puerto = %d\n", 
            i + 1, router.peers[i].id, router.peers[i].ip, router.peers[i].port);
    }
    printf("\n");

    fclose(archivo);
    return router;
}

void *listening(void *arg) {
    // Configuración del buffer y socket del remitente (routers vecinos)
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
 
    printf("[ROUTER] Escuchando...\n");
 
    // Mantiene la escucha activa
    while (running) {
        // Recibimiento de bytes de vecinos (si es que alguno manda)
        memset(buffer, 0, MAX_BUFFER_SIZE);
        int bytes = recvfrom(sock_fd, buffer, MAX_BUFFER_SIZE - 1, 0, (struct sockaddr *)&sender, &sender_len);
 
        // Bytes fueron recibidos, se descubre la IP del envío
        if (bytes > 0) {
            char ip_sender[16];
            inet_ntop(AF_INET, &sender.sin_addr, ip_sender, sizeof(ip_sender));
            printf("[ROUTER] Mensaje de %s:%d -> %s\n", ip_sender, ntohs(sender.sin_port), buffer);
        }
    }
 
    return NULL;
}

//static void sendInitialMsg(ConfigRouter router) {
void sendInitialMsg(ConfigRouter router) {
    // Configuración del mensaje de broadcast de acuerdo al protocolo
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), "ANNOUNCE|%s|%d|%s", router.ip, router.port, router.id);
    
    //pr_info("[ROUTER] Enviando saludo inicial a %d vecinos\n", router.num_peers);

    // Envío del mensaje a cada vecino vía kernel
    for (int i = 0; i < router.num_peers; i++) {
        Peer peer = router.peers[i];

        // Configuración para el envío (vecino destino) 
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(peer.port);
        inet_pton(AF_INET, peer.ip, &dest.sin_addr);

        // Envío del mensaje inicial a routers vecinos
        sendto(sock_fd, message, strlen(message), 0, (struct sockaddr *)&dest, sizeof(dest));
        printf("[ROUTER] Saludo enviado a %s (%s:%d)\n", peer.id, peer.ip, peer.port);
    }
}

pthread_t activateRouter(void) {
    ConfigRouter router = initRouter();
 
    // Creación y configuración del socket UDP
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(router.port);
 
    // Binding del socket UDP
    bind(sock_fd, (struct sockaddr *)&address, sizeof(address));
 
    // Hilo de escucha (Así el router no acapara todo el funcionamiento del programa)
    pthread_t listener_thread;
    pthread_create(&listener_thread, NULL, listening, NULL);
 
    // Anunciarse a los vecinos
    sendInitialMsg(router);
 
    return listener_thread;
}*/