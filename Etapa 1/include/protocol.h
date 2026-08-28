#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// --- Tamaños y Configuración de Red ---
#define DEFAULT_PORT 8080
#define MAX_PAYLOAD_SIZE 512
#define MAX_BUFFER_SIZE (sizeof(Header) + MAX_PAYLOAD_SIZE)

// --- Tipos de Trama ---
typedef enum {
    FRAME_DATA = 0,
    FRAME_ACK  = 1,
    FRAME_END  = 2
} FrameType;

// --- Encabezado del Protocolo (4 bytes) ---
#pragma pack(push, 1)
typedef struct {
    uint8_t type;           // DATOS (0), ACK (1), END (2)
    uint8_t seqNumber;     // Bit alternado (0 o 1)
    uint16_t payloadLength;// Bytes útiles en la trama
} Header;
#pragma pack(pop)

#endif // PROTOCOL_H