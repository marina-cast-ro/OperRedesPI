#include "../include/listener.h"
#include "../include/sender.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *programName){
    fprintf(stderr, "Uso:\n");
    fprintf(stderr, "  %s --listen <routerIp> <routerPort>\n", programName);
    fprintf(stderr, "  %s --send <routerIp> <routerPort> <destIp> <mensaje>\n", programName);
}

static int versionListen(int argc, char *argv[]){
    if (argc != 4){
        fprintf(stderr, "Uso: %s --listen <routerIp> <routerPort>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *routerIp = argv[2];
    int routerPort = atoi(argv[3]);

    if(keepListening(routerIp, routerPort) != 0){
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

    if (sendMessage(routerIp, routerPort, destIp, message) != 0){
        fprintf(stderr, "Error al enviar el mensaje\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--listen") == 0) {
        return versionListen(argc, argv);

    } else if (strcmp(argv[1], "--send") == 0) {
        return versionSend(argc, argv);

    } else {
        fprintf(stderr, "Modo desconocido: %s\n", argv[1]);
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
}