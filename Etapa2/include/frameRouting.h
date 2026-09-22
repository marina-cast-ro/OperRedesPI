#ifndef FRAME_ROUTING_H
#define FRAME_ROUTING_H

#include <stddef.h>
#include <stdint.h>

// TIpo de mensaje
typedef enum {
    ROUTING_ANNOUNCEMENT, /*Cuando un nodo anuncia una ip*/
    ROUTING_DATA /*Cuando us un mensaje con datos para un destinario*/
} RoutingMessageType;

// Datos que se obtienen al leer un paquete.
typedef struct {
    RoutingMessageType type;
    uint32_t sourceIp; 
    uint32_t destinationIp;
} RoutingMessage;

// Datos del vecino al que podemos enviar un paquete .
typedef struct {
    uint32_t nextHopIp;        // IP del vecino.
    uint16_t port;             // Puerto donde el vecino escucha.
    unsigned int interfaceId;  // Numero que identifica el enlace en ese router.
} RoutingLink;

// Nombres para saber que paso al procesar un paquete.
typedef enum {
    FRAME_ROUTE_SAVED = 0,
    FRAME_FORWARDED = 1,
    FRAME_INVALID = -1,
    FRAME_MEMORY_ERROR = -3,
    FRAME_NO_ROUTE = -4,
    FRAME_SEND_ERROR = -5
} FrameRoutingResult;

// Modulo1, punto4: Guardar la IP anunciada y por donde llegar a ella.
FrameRoutingResult registerAnnouncement(uint32_t announcedIp
, const RoutingLink *incomingLink);

// Modulo1, punto5: consultar  ruta y enviar el paquete al vecino indicado.
FrameRoutingResult forwardMessage(uint32_t destinationIp
, const void *frame, size_t length);

// Funcion para procesar el paquete. 
// incomingLink indica la IP y puerto de escucha del vecino;
// puede ser NULL si el paquete es de datos.
FrameRoutingResult processReceivedFrame(const void *frame, size_t length
, const RoutingLink *incomingLink);


// TODO verificar el paquete y escribir su tipo e IP en message.
// Esto porque se ncesuta saber el formato del paquete que acordemos en el protocolooooooooooooooooooooooo
int decodeFrame(const void *frame, size_t length, RoutingMessage *message);
    // Verificar el paquete.
    // Leer el tipo y las IP según el formato .
    // Guardar esos datos en message.
    // Devolver el resultado.

// TODO Modulo 2 copiar la IP y el enlace a la memoria simulada usando la MMU.

// TODO Modulo 2 buscar la ruta usando la MMU y escribir el resultado en link.


// Se le pide al sistema operativo enviar el paquete con la syscall
long syscallSendFrame(const char *ip, int port, const void *frame, size_t length);

#endif  // FRAME_ROUTING_H
