#include "configParser.h"

#include <stdio.h>      // Para FILE, fopen, fgets, fclose, sscanf, perror
#include <stdlib.h>     // Para atoi
#include <string.h>     // Para strncmp, strcspn
#include <arpa/inet.h>  // Para struct in_addr e inet_pton


static uint32_t savedLocalIp = 0;  // La IP propia, guardada para el ANNOUNCE

uint32_t getLocalIp(void) {
    return savedLocalIp;
}

// Lee config.txt y precarga los vecinos en la MMU con saveRoute()
ConfigRouter parseConfigAndPreload(const char *filename) {
    ConfigRouter router;
    router.localIp = 0;
    router.port = ERROR_ROUTER;
    router.neighborCount = 0;

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo de configuración");
        return router;
    }   

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0'; // Limpia saltos de línea

        // Ignora comentarios o líneas vacías
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // --- Lee: local_ip ---
        if (strncmp(line, "local_ip:", 9) == 0) {
            char ip_str[16];
            if (sscanf(line + 9, " %15s", ip_str) == 1) {
                struct in_addr addr;
                if (inet_pton(AF_INET, ip_str, &addr) == 1) {
                    router.localIp = addr.s_addr;
                }
            }
        }

        // --- Lee: port ---
        else if (strncmp(line, "port:", 5) == 0) {
            router.port = atoi(line + 5);
        }

        // --- Lee: neighbor (IP real y puerto de un router vecino) ---
        else if (strncmp(line, "neighbor:", 9) == 0 && router.neighborCount < MAX_CONFIG_NEIGHBORS) {
            int n = router.neighborCount;
            if (sscanf(line + 9, " %15s %d", router.neighborIp[n], &router.neighborPort[n]) == 2) {
                router.neighborCount++;
            }
        }

        // --- Precarga en la MMU: host ---
        else if (strncmp(line, "host:", 5) == 0) {
            char ip_str[16];
            uint32_t interface;
            if (sscanf(line + 5, " %15[^:]:%u", ip_str, &interface) == 2) {
                struct in_addr addr;
                if (inet_pton(AF_INET, ip_str, &addr) == 1) {
                    saveRoute(addr.s_addr, interface);
                }
            }
        }
    }

    fclose(file);
    savedLocalIp = router.localIp;
    return router;
}