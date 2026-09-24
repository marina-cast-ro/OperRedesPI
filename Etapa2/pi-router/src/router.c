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

ConfigRouter initRouter(void) {
    ConfigRouter router;
    router.num_peers = 0;
    
    // Lectura del archivo de configuración del router
    FILE *archivo = fopen("../config.txt", "r");

    if (!archivo) {
        printf("[ROUTER] Error al abrir el archivo de configuración <config.txt>");
        exit(1);
    }

    // VERSIÓN N° 1: Se puede simplificar

    // Contador de vecinos como informe general
    /**int peer_count = 0;

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        // 1. Identificador del router propio
        if (strncmp(linea, "router_id:", 10) == 0) {
            char router_id[MAX_NAME_ROUTER];
            sscanf(linea, "router_id: %s", router_id);
            strcpy(router.id, router_id);
        } 
        // 2. Puerto del router propio
        else if (strncmp(linea, "puerto:", 7) == 0) {
            int port;
            sscanf(linea, "puerto: %d", &port);
            router.port = port;
        }
        // 3. Vecinos directos del router
        else if (strncmp(linea, "vecino:", 7) == 0) {
            char router_id[MAX_NAME_ROUTER];
            char ip[15];
            int port;
            sscanf(linea, "vecino: %s %s %d", router_id, ip, &port);
            
            strcpy(router.peers[peer_count].id, router_id);
            strcpy(router.peers[peer_count].ip, ip);
            router.peers[peer_count].port = port;

            peer_count++;
        }
    }

    router.num_peers = peer_count;*/

    // VERSIÓN N° 2: Simplificado y mejorado
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

// Versión vieja: Con llamados del kernel, creo que aquí me compliqué y se 
// puede hacer mejor en modo usuario
/**static int listening(ConfigRouter router) {
    char buffer[MAX_BUFFER_SIZE];
    int received_bytes;

    pr_info("[ROUTER] Escuchando en puerto %d\n", router.port);

    while (!kthread_should_stop()) {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        // Timeout 1000ms para revisar kthread_should_stop()
        received_bytes = ksocket_recvfrom(listen_socket, buffer, MAX_BUFFER_SIZE - 1, 1000);

        // Algún conjunto de bytes fueron recibidos
        if (received_bytes > 0) {
            pr_info("[ROUTER] Mensaje recibido (%d bytes): %s\n", received_bytes, buffer);
            // TODO integracion Modulo 1: pasar buffer y received_bytes a
            // processReceivedFrame (Etapa2/include/frameRouting.h), junto con
            // el enlace de retorno del emisor. Faltan las funciones de memoria,
            // lectura de protocol.h y syscallSendFrame. Ver Etapa2/MODULO1.md.
            // Antes hay que resolver la mezcla kernel/user space de esta
            // escucha: frameRouting y la MMU actual trabajan en user space.
            // ksocket_recvfrom no expone el emisor; sockets debe identificar
            // su enlace/puerto de escucha, sin asumir que sea el puerto origen.
        } 
        // Error distinto a timeout, espera intencional para reintentar
        else if (received_bytes != -EAGAIN) {
            msleep(100);
        }
    }

    pr_info("[ROUTER] Escucha de routers vecinos detenida\n");
    return 0;
}*/

void *listening(void *arg) {
    // Configuración del buffer y socket del remitente (routers vecinos)
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
 
    printf("[ROUTER] Escuchando...\n");
 
    // Mantiene la escucha activa
    while (true) {
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

static void endRouterListen(ConfigRouter router) {
    if (listen_thread) {
        kthread_stop(listen_thread);
    }

    ksocketRelease(listen_socket);
    pr_info("[ROUTER] Finalización de escucha del router\n");
}*/

//static void sendInitialMsg(ConfigRouter router) {
void sendInitialMsg(ConfigRouter router) {
    // Configuración del mensaje de broadcast de acuerdo al protocolo
    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), "ANNOUNCE|%s|%d|%s", router.ip, router.port, router.id);
    
    //pr_info("[ROUTER] Enviando saludo inicial a %d vecinos\n", router.num_peers);

    // Envío del mensaje a cada vecino vía kernel
    for (int i = 0; i < router.num_peers; i++) {
        Peer peer = router.peers[i];

        // Versión vieja
        /*int error = ksocket_sendto(listen_socket, peer.ip, peer.port, message, strlen(message));
        if (error < 0) {
            pr_err("[ROUTER] Fallo al saludar a %s (%s:%d)\n", peer.id, peer.ip, peer.port);
        } else {
            pr_info("[ROUTER] Saludo enviado a %s (%s:%d)\n", peer.id, peer.ip, peer.port);
        }*/

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
}