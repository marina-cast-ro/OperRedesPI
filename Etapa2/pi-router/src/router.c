#include "router.h"

ConfigRouter initRouter(void) {
    ConfigRouter router;
    
    // Lectura del archivo de configuración del router
    FILE *archivo = fopen("../config.txt", "r");

    if (!archivo) {
        printf("[ROUTER] Error al abrir el archivo de configuración <config.txt>");
        router.port = ERROR_ROUTER;
        return router;
    }

    // Contador de vecinos como informe general
    int peer_count = 0;

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

    router.num_peers = peer_count;

    // Revisión de los datos guardados en el Router
    printf("================ DATOS DEL ROUTER ================\n");
    printf("Router ID: %s | Router IP: %d\n", router.id, router.port);
    printf("Cantidad de vecinos encontrados: %d\n", router.num_peers);
    
    for (int i = 0; i < router.num_peers; i++) {
        printf("Vecino #%d: ID = %s | IP = %s | Puerto = %d\n", 
            i + 1, router.peers[i].id, router.peers[i].ip, router.peers[i].port);
    }
    printf("\n");

    fclose(archivo);
    return router;
}

static int listening(ConfigRouter router) {
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
}

static int initRouterListen(ConfigRouter router) {
    pr_info("[ROUTER] Activando router con el puerto %d\n", router.port);

    // Creación del socket del router para la escucha
    int error = ksocketCreate(&listen_socket, router.port);
    if (error < 0) {
        pr_err("[ROUTER] Error al crear el socket de escucha\n");
        return error;
    }

    // Creación de un hilo encargado de la escucha para el router
    listen_thread = kthread_run(listening, NULL, "router_listen_thread");
    if (IS_ERR(listen_thread)) {
        pr_err("[ROUTER] Error al crear el hilo de escucha\n");
        ksocketRelease(listen_socket);
        return PTR_ERR(listen_thread);
    }

    return 0;
}

static void endRouterListen(ConfigRouter router) {
    if (listen_thread) {
        kthread_stop(listen_thread);
    }

    ksocketRelease(listen_socket);
    pr_info("[ROUTER] Finalización de escucha del router\n");
}

static void sendInitialMsg(ConfigRouter router) {
    char message[MAX_BUFFER_SIZE];

    // Configuración del mensaje de broadcast de acuerdo al protocolo
    snprintf(message, sizeof(message), "ANNOUNCE|%s|%d|%s", router.ip, router.port, router.id);
    pr_info("[ROUTER] Enviando saludo inicial a %d vecinos\n", router.num_peers);

    // Envío del mensaje a cada vecino vía kernel
    for (int i = 0; i < router.num_peers; i++) {
        Peer peer = router.peers[i];

        int error = ksocket_sendto(listen_socket, peer.ip, peer.port, message, strlen(message));

        if (error < 0) {
            pr_err("[ROUTER] Fallo al saludar a %s (%s:%d)\n", peer.id, peer.ip, peer.port);
        } else {
            pr_info("[ROUTER] Saludo enviado a %s (%s:%d)\n", peer.id, peer.ip, peer.port);
        }
    }
}