#include "routingTable.h"

// Cada ruta ocupa 8 bytes seguidos en la memoria virtual, o sea una página entera de la MMU:
//   bytes 0-3: IP destino | bytes 4-7: interfaz por la que se reenvía
// Una ruta con IP destino 0 está libre, porque la memoria arranca en ceros.
#define ROUTE_SIZE      8
#define ROUTE_CAPACITY  (MEMORY_SIZE / ROUTE_SIZE)  // 32 rutas

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

// Escribe un número en count bytes seguidos (count <= 4). El último byte es el más bajo
static void writeNumber(uint32_t address, uint32_t value, int count) {
    for (int i = count - 1; i >= 0; i--) {
        mmuWriteByte(address + i, value & 0xFF);
        value >>= 8;
    }
}

int saveRoute(uint32_t destinationIp, uint32_t interfaceId) {
    if (destinationIp == 0) {
        return -1;  // La IP 0 es la marca de ruta libre
    }

    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        // Se escribe en la primera ruta libre o actualiza si ya existe
        if (storedIp == 0 || storedIp == destinationIp) {
            // Escribir IP (bytes 0-3)
            writeNumber(routeAddress, destinationIp, 4);
            writeNumber(routeAddress + 4, interfaceId, 4);
            return 0;
        }
    }
    return -1;  // Tabla llena
}

int findRoute(uint32_t destinationIp, uint32_t *outInterfaceId) {
    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        if (storedIp == 0) {
            return -1;  // Fin de rutas guardadas
        }
        
        if (storedIp == destinationIp) {
            *outInterfaceId = readNumber(routeAddress + 4, 4);
            return 0;
        }
    }
    return -1;  // No está en la tabla
}