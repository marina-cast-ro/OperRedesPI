#ifndef ETAPA2_PROTOCOL_H
#define ETAPA2_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>


/*
EJEMPLOs DE LA PIZARRA==============================

    [ANNOUNCE|IP_ORIGEN] -> Que los vecinos nos descubran   
    [ADVERTISE|IP_A_PROPAGAR] -> Propagar IP a los destinos   
    [DATA|IP_DESTINO|DATOS] -> comunicarnos

*/

// Texto para construir los mensajes.
#define PROTOCOL_ANNOUNCE "ANNOUNCE"
#define PROTOCOL_ADVERTISE "ADVERTISE"
#define PROTOCOL_DATA "DATA"
#define PROTOCOL_SEPARATOR '|'

// Tipo de mensaje.
typedef enum {
    ROUTING_ANNOUNCEMENT,
    ROUTING_DATA,
    ROUTING_ADVERTISEMENT
} RoutingMessageType;

// Estructura para almacenar el mensaje.Esta no se envia por la red.
typedef struct {
    RoutingMessageType type;
    uint32_t announcedIp;    // IP de ANNOUNCE o ADVERTISE
    uint32_t destinationIp;  // IP de DATA
    const char *data;        // Datos de DATA.
    size_t dataLength;
} RoutingMessage;

// Lee y valida un mensaje completo usando length, sin '\0', '\r' ni '\n'.
// Rechaza la IP 0.0.0.0. En DATA, lo posterior al segundo '|' son los datos.
// data apunta al paquete original: conservarlo y leer usando dataLength.
int decodeFrame(const void *frame, size_t length, RoutingMessage *message);

#endif  // ETAPA2_PROTOCOL_H
