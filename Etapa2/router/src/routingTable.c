#include "routingTable.h"
#include <arpa/inet.h>

#define ROUTE_SIZE      8
#define ROUTE_CAPACITY  (MEMORY_SIZE / ROUTE_SIZE)  // 32 rutas

static uint32_t readNumber(uint32_t address, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; i++) {
        uint8_t byte = 0;
        mmuReadByte(address + (uint32_t)i, &byte);
        value = (value << 8) | byte;
    }
    return value;
}

static void writeNumber(uint32_t address, uint32_t value, int count) {
    for (int i = count - 1; i >= 0; i--) {
        mmuWriteByte(address + (uint32_t)i, value & 0xFF);
        value >>= 8;
    }
}

int saveRoute(uint32_t destinationIp, uint32_t interfaceId) {
    if (destinationIp == 0) {
        return -1;
    }

    // Convertimos de Network Order a Host Order para almacenamiento uniforme
    uint32_t hostIp = ntohl(destinationIp);
    int firstFreeIndex = -1;

    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = (uint32_t)i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        // Si la IP ya existe en la tabla, actualizamos su interfaz de salida
        if (storedIp == hostIp) {
            writeNumber(routeAddress + 4, interfaceId, 4);
            return 0;
        }

        // Guardamos la ubicación de la primera ranura libre
        if (storedIp == 0 && firstFreeIndex == -1) {
            firstFreeIndex = i;
        }
    }

    // Si la ruta no existía, la escribimos en la primera ranura libre encontrada
    if (firstFreeIndex != -1) {
        uint32_t freeAddress = (uint32_t)firstFreeIndex * ROUTE_SIZE;
        writeNumber(freeAddress, hostIp, 4);
        writeNumber(freeAddress + 4, interfaceId, 4);
        return 0;
    }

    return -1; // Tabla llena
}

int findRoute(uint32_t destinationIp, uint32_t *outInterfaceId) {
    if (destinationIp == 0) return -1;

    uint32_t hostIp = ntohl(destinationIp);

    // Escaneo completo de la tabla sin frenar en 0 para evitar saltos por huecos
    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = (uint32_t)i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        if (storedIp == hostIp) {
            *outInterfaceId = readNumber(routeAddress + 4, 4);
            return 0;
        }
    }
    return -1; // No encontrada
}

int getRouteIp(int index, uint32_t *destinationIp) {
    if (index < 0 || index >= ROUTE_CAPACITY) {
        return -1;
    }

    uint32_t storedIp = readNumber((uint32_t)index * ROUTE_SIZE, 4);
    if (storedIp == 0) {
        return -1;
    }

    // Retornamos la IP convertida de vuelta a Network Byte Order
    *destinationIp = htonl(storedIp);
    return 0;
}