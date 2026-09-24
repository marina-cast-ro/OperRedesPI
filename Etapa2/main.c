#include "listener.h"
#include "sender.h"
#include "router.h"
#include "virtualMemory.h"
#include "configParser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void printUsage(const char *programName){
    fprintf(stderr, "Uso:\n");
    fprintf(stderr, "  %s --listen <routerIp> <routerPort>\n", programName);
    fprintf(stderr, "  %s --send <routerIp> <routerPort> <destIp> <mensaje>\n", programName);
    fprintf(stderr, "  %s --router <ruta-al-config.txt>\n", programName);
}

static int versionListen(int argc, char *argv[]){
    if (argc < 4 || argc > 5){
        fprintf(stderr, "Uso: %s --listen <routerIp> <routerPort> [hostLogicalIp]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *routerIp = argv[2];
    int routerPort = atoi(argv[3]);
    // Si se pasa el argumento lo usa, si no, usa "10.0.0.100" por defecto
    const char *hostLogicalIp = (argc == 5) ? argv[4] : "10.0.0.100";

    if(keepListening(routerIp, routerPort, hostLogicalIp) != 0){
        fprintf(stderr, "Error al escuchar\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int versionSend(int argc, char *argv[]){
    if(argc != 6){
        fprintf(stderr, "Uso: %s --send <routerIp> <routerPort> <destIp> <mensaje>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *routerIp = argv[2];
    int routerPort = atoi(argv[3]);
    const char *destIp = argv[4];
    const char *message = argv[5];

    if(sendMessage(routerIp, routerPort, destIp, message) != 0){
        fprintf(stderr, "Error al enviar el mensaje\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int versionRouter(int argc, char *argv[]){
    if(argc != 3){
        fprintf(stderr, "  %s --router <ruta-al-config.txt>\n", argv[0]);        
        return EXIT_FAILURE;
    }
    initMMU();
    virtualMemoryInit();

    const char *configPath = argv[2];

    ConfigRouter router = parseConfigAndPreload(configPath);
    if(router.port == ERROR_ROUTER){
        fprintf(stderr, "Error al leer el archivo de configuración: %s\n", configPath);
        return EXIT_FAILURE;
    }

    if(initRouterListen(router) != 0){
        fprintf(stderr, "Error al iniciar el router\n");
        return EXIT_FAILURE;
    }

    printf("Router ejecutando en el puerto... %d.\n", router.port);
    while(1){
        sleep(1);//esto permite mantener el proceso principal vivo, mientras el hilo escucha
    }

    //FALTA CERRAR ROUTER
    //endRouterListen(router);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--listen") == 0){
        return versionListen(argc, argv);

    } else if (strcmp(argv[1], "--send") == 0){
        return versionSend(argc, argv);

    } else if (strcmp(argv[1], "--router") == 0) {
        return versionRouter(argc, argv);

    } else {
        fprintf(stderr, "Modo desconocido: %s\n", argv[1]);
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
}