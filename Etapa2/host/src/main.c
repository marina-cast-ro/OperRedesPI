#include "protocol.h"
#include "sender.h"
#include "listener.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <unistd.h>

static void printUsage(const char *programName) {
    fprintf(stderr,
        "Uso:\n"
        "  %s --router-ip <ip> --router-port <puerto> --enviar <destIp> <destPuerto> <mensaje>\n"
        "  %s --router-ip <ip> --router-port <puerto> --escuchar <puertoLocal>\n",
        programName, programName);
}

int main(int argc, char *argv[]) {
    const char *routerIp = NULL;
    int routerPort = 0;
    const char *myIp = "127.0.0.1"; // TODO: detectar la IP real del host

    int modoEnviar = 0, modoEscuchar = 0;
    const char *destIp = NULL;
    int destPort = 0;
    const char *message = NULL;
    int localPort = 0;

    static struct option longOptions[] = {
        {"router-ip",   required_argument, 0, 'r'},
        {"router-port", required_argument, 0, 'p'},
        {"enviar",      no_argument,       0, 'e'},
        {"escuchar",    no_argument,       0, 'l'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "r:p:el", longOptions, NULL)) != -1) {
        switch (opt) {
            case 'r': routerIp = optarg; break;
            case 'p': routerPort = atoi(optarg); break;
            case 'e': modoEnviar = 1; break;
            case 'l': modoEscuchar = 1; break;
            default: printUsage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (!routerIp || routerPort == 0 || (!modoEnviar && !modoEscuchar) || (modoEnviar && modoEscuchar)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Argumentos posicionales restantes (después de las opciones)
    if (modoEnviar) {
        if (argc - optind < 3) { printUsage(argv[0]); return EXIT_FAILURE; }
        destIp = argv[optind];
        destPort = atoi(argv[optind + 1]);
        message = argv[optind + 2];
    } else {
        if (argc - optind < 1) { printUsage(argv[0]); return EXIT_FAILURE; }
        localPort = atoi(argv[optind]);
    }

    // 2. Dejar el socket persistente del kernel listo
    if (syscall(SYS_INIT_PROTOCOL) < 0) {
        perror("SYS_INIT_PROTOCOL");
        fprintf(stderr, "No se pudo inicializar el protocolo.\n");
    }

    // 3. Anunciar la IP al router (ambos modos)
    if (greetAndMeet(routerIp, routerPort, myIp) < 0) {
        fprintf(stderr, "No se pudo anunciar la IP al router\n");
    }

    // 4. Bifurcación según el modo
    if (modoEnviar) {
        if (sendMessage(routerIp, routerPort, destIp, destPort, message) < 0) {
            fprintf(stderr, "Fallo el envio, reintentando init...\n");
            syscall(SYS_INIT_PROTOCOL);
            if (sendMessage(routerIp, routerPort, destIp, destPort, message) < 0) {
                fprintf(stderr, "Fallo el envio tras reintento\n");
                return EXIT_FAILURE;
            }
        }
        printf("Mensaje enviado\n");
    } else {
        keepListening(localPort);
    }

    return EXIT_SUCCESS;
}