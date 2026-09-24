#include <stdio.h>
#include <string.h>
#include "mmu.h"
#include "virtualMemory.h"
#include "configParser.h"
#include "router.h"
#include "forwarding.h"
#include "../../host/include/listener.h"
#include "../../host/include/sender.h"


// --- MAIN PRINCIPAL DE EJEMPLO ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s [router | receptor | emisor] [args...]\n", argv[0]);
        return -1;
    }

    // ==========================================
    // MODO 1: ROUTER
    // ==========================================
    if (strcmp(argv[1], "router") == 0) {
        // 1. Inicializar la memoria física y virtual ANTES de cualquier cosa
        initMMU();
        virtualMemoryInit();

        // 2. Leer config y precargar interfaces virtuales en la MMU
        ConfigRouter routerConfig = parseConfigAndPreload("config.txt");

        // 3. Levantar la red: Inicia el hilo que hace accept() y recibe datos
        if (initRouterListen(routerConfig) == ERROR_ROUTER) {
            return -1;
        }

        // 4. Propagación inicial: Ya con la red activa y la MMU precargada, avisar a vecinos
        //sendInitialAnnounce();  // Faltan cosas de esta función
        sendInitialAdvertise();

        // 5. Mantener vivo el programa principal (el router corre en un hilo secundario)
        while(1) {
            sleep(10); 
        }
    } 
    
    // ==========================================
    // MODO 2: RECEPTOR (Host pasivo)
    // ==========================================
    else if (strcmp(argv[1], "receptor") == 0) {
        // Ejecuta SOLO la lógica del listener. NO inicializa MMU.
        // Asume que argv trae la IP y puerto del router.
        const char *routerIp = argv[2];
        int routerPort = atoi(argv[3]);
        
        keepListening(routerIp, routerPort);
    } 
    
    // ==========================================
    // MODO 3: EMISOR (Host activo)
    // ==========================================
    else if (strcmp(argv[1], "emisor") == 0) {
        // Ejecuta SOLO la lógica de envío de un mensaje. NO inicializa MMU.
        const char *routerIp = argv[2];
        int routerPort = atoi(argv[3]);
        const char *destIp = argv[4];
        const char *msg = argv[5];

        sendMessage(routerIp, routerPort, destIp, msg);
    }

    return 0;
}