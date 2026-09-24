#include "configParser.h"

#include <stdio.h>      // Para FILE, fopen, fgets, fclose, sscanf, perror
#include <stdlib.h>     // Para atoi
#include <string.h>     // Para strncmp, strcspn
#include <arpa/inet.h>  // Para struct in_addr e inet_pton


// Lee config.txt y precarga los vecinos en la MMU con saveRoute()
ConfigRouter parseConfigAndPreload(const char *filename) {
    ConfigRouter router;
    router.localIp = 0;
    router.port = ERROR_ROUTER;

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
    return router;
}