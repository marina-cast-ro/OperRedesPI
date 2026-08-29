#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __KERNEL__
    /* --- Headers para Kernel Space --- */
    #include <linux/types.h>
	#include <linux/net.h>
	#include <linux/in.h>

	// Compatibilidad de tipos stdint en el Kernel
    typedef u8  uint8_t;
    typedef u16 uint16_t;
#else
    /* --- Headers para User Space --- */
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>
    #include <arpa/inet.h>
#endif

// --- Tamaños y Configuración de Red ---
#define DEFAULT_PORT 8080
#define MAX_PAYLOAD_SIZE 512

// --- Tipos de Trama ---
typedef enum {
    PROTOCOL_FRAME_DATA = 0,
    PROTOCOL_FRAME_ACK  = 1,
    PROTOCOL_FRAME_END  = 2
} FrameType;

// --- Encabezado del Protocolo (4 bytes) ---
#pragma pack(push, 1)
typedef struct {
    uint8_t type;           // DATOS (0), ACK (1), END (2)
    uint8_t seqNumber;      // Bit alternado (0 o 1)
    uint16_t payloadLength; // Bytes útiles en la trama
} Header;
#pragma pack(pop)

// --- Estructura de Frame (516 bytes) ---
#pragma pack(push, 1)
typedef struct {
    Header header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} Frame;
#pragma pack(pop)

#define MAX_BUFFER_SIZE (sizeof(Header) + MAX_PAYLOAD_SIZE)

#endif // PROTOCOL_H