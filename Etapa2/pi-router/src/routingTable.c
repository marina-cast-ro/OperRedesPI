#include "routingTable.h"

// Cada ruta ocupa 10 bytes seguidos en la memoria virtual:
//   bytes 0-3: IP destino | bytes 4-7: IP del vecino | bytes 8-9: puerto del vecino
// Una ruta con IP destino 0 está libre, porque la memoria arranca en ceros.
#define ROUTE_SIZE      10
#define ROUTE_CAPACITY  (MEMORY_SIZE / ROUTE_SIZE)  // 25 rutas

// Lee count bytes seguidos (count <= 4) y los junta en un número. El primero es el más alto
static uint32_t readNumber(uint32_t address, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; i++) {
        uint8_t byte = 0;
        mmuReadByte(address + i, &byte);
        value = (value << 8) | byte;
    }
    return value;
}

// Escribe los count bytes más bajos de value (count <= 4). El más alto va primero
static void writeNumber(uint32_t address, uint32_t value, int count) {
    for (int i = count - 1; i >= 0; i--) {
        mmuWriteByte(address + i, value & 0xFF);
        value >>= 8;
    }
}

int saveRoute(uint32_t destinationIp, uint32_t neighborIp, uint16_t neighborPort) {
    if (destinationIp == 0) {
        return -1;  // La IP 0 es la marca de ruta libre
    }

    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        // Se escribe en la primera ruta libre, o encima de la misma IP si ya estaba
        if (storedIp == 0 || storedIp == destinationIp) {
            writeNumber(routeAddress, destinationIp, 4);
            writeNumber(routeAddress + 4, neighborIp, 4);
            writeNumber(routeAddress + 8, neighborPort, 2);
            return 0;
        }
    }
    return -1;  // Tabla llena
}

int findRoute(uint32_t destinationIp, uint32_t *neighborIp, uint16_t *neighborPort) {
    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        if (storedIp == 0) {
            return -1;  // Las rutas se guardan seguidas, así que la primera libre es el final
        }
        if (storedIp == destinationIp) {
            *neighborIp = readNumber(routeAddress + 4, 4);
            *neighborPort = readNumber(routeAddress + 8, 2);
            return 0;
        }
    }
    return -1;  // Tabla llena y ninguna ruta era hacia destinationIp
}