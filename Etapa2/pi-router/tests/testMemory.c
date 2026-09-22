#include "routingTable.h"

// Arma una IP como número: IP(10, 0, 0, 2) es 10.0.0.2
#define IP(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

int main(void) {
    uint32_t neighborIp = 0;
    uint16_t neighborPort = 0;
    virtualMemoryInit();

    // Para llegar a 192.168.1.30 hay que pasarle el paquete al vecino 10.0.0.2:5001
    saveRoute(IP(192, 168, 1, 30), IP(10, 0, 0, 2), 5001);

    if (findRoute(IP(192, 168, 1, 30), &neighborIp, &neighborPort) == 0 &&
        neighborIp == IP(10, 0, 0, 2) && neighborPort == 5001) {
        printf("OK: la ruta a 192.168.1.30 sale por 10.0.0.2:5001\n");
    } else {
        printf("FALLA: no se encontró la ruta guardada\n");
    }

    if (findRoute(IP(192, 168, 1, 99), &neighborIp, &neighborPort) == -1) {
        printf("OK: 192.168.1.99 no tiene ruta\n");
    } else {
        printf("FALLA: 192.168.1.99 no debería tener ruta\n");
    }
    return 0;
}