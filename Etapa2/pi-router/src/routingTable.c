#include "routingTable.h"

// Cada ruta ocupa exactamente 8 bytes (1 página de la MMU):
//   bytes 0-3: IP destino (uint32_t) | bytes 4-7: Nombre de interfaz ("eth0\0", "eth1\0")
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

// Escribe los count bytes más bajos de value (count <= 4). El más alto va primero
static void writeNumber(uint32_t address, uint32_t value, int count) {
    for (int i = count - 1; i >= 0; i--) {
        mmuWriteByte(address + i, value & 0xFF);
        value >>= 8;
    }
}

int saveRoute(uint32_t destinationIp, const char *interfaz) {
    if (destinationIp == 0 || interfaz == NULL) {
        return -1;  // La IP 0 es la marca de ruta libre
    }

    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        // Se escribe en la primera ruta libre o actualiza si ya existe
        if (storedIp == 0 || storedIp == destinationIp) {
            // Escribir IP (bytes 0-3)
            writeNumber(routeAddress, destinationIp, 4);
            
            // Escribir string de Interfaz byte a byte (bytes 4-7)
            for (int j = 0; j < 4; j++) {
                uint8_t c = (j < strlen(interfaz)) ? (uint8_t)interfaz[j] : 0;
                mmuWriteByte(routeAddress + 4 + j, c);
            }
            return 0;
        }
    }
    return -1;  // Tabla llena
}

int findRoute(uint32_t destinationIp, char *outInterfaz) {
    if (destinationIp == 0 || outInterfaz == NULL) {
        return -1;
    }

    for (int i = 0; i < ROUTE_CAPACITY; i++) {
        uint32_t routeAddress = i * ROUTE_SIZE;
        uint32_t storedIp = readNumber(routeAddress, 4);

        if (storedIp == 0) {
            return -1;  // Fin de rutas guardadas
        }
        
        if (storedIp == destinationIp) {
            // Leer string de Interfaz byte a byte (bytes 4-7)
            for (int j = 0; j < 4; j++) {
                uint8_t byte = 0;
                mmuReadByte(routeAddress + 4 + j, &byte);
                outInterfaz[j] = (char)byte;
            }
            outInterfaz[4] = '\0'; // Asegurar fin de cadena
            return 0;
        }
    }
    return -1;  // Tabla llena y ninguna ruta era hacia destinationIp
}