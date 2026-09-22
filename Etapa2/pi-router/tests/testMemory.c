#include "routingTable.h"

// Arma una IP como número: IP(10, 0, 0, 2) es 10.0.0.2
#define IP(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

int main(void) {
    uint32_t interfaceId = 0;
    virtualMemoryInit();

    // Para llegar a 192.168.1.30 hay que reenviar el mensaje por la interfaz 1
    saveRoute(IP(192, 168, 1, 30), 1);

    if (findRoute(IP(192, 168, 1, 30), &interfaceId) == 0 && interfaceId == 1) {
        printf("OK: la ruta a 192.168.1.30 sale por la interfaz 1\n");
    } else {
        printf("FALLA: no se encontró la ruta guardada\n");
    }

    if (findRoute(IP(192, 168, 1, 99), &interfaceId) == -1) {
        printf("OK: 192.168.1.99 no tiene ruta\n");
    } else {
        printf("FALLA: 192.168.1.99 no debería tener ruta\n");
    }
    return 0;
}