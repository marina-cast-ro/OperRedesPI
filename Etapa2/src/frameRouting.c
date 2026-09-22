#include "frameRouting.h"

#include <arpa/inet.h>

// Recibe un paquete y decide cual de tus dos tareas debe ejecutar.
FrameRoutingResult processReceivedFrame(
    const void *frame, size_t length, const RoutingLink *incomingLink) {
    // Se verifica que exista un paquete y que no este vacio.
    if (frame == NULL || length == 0) {
        return FRAME_INVALID;
    }

    // Se pide leer el tipo de mensaje y las IP del paquete.
    // TODO implementar decodeFrame
    RoutingMessage message = {0};
    if (decodeFrame(frame, length, &message) != 0) {
        return FRAME_INVALID;
    }

    // Si alguien anuncia su IP, se pide guardar como llegar hasta el.
    if (message.type == ROUTING_ANNOUNCEMENT) {
        return registerAnnouncement(message.sourceIp, incomingLink);
    }
    // Si trae datos para un destinatario, se intenta reenviarlos.
    if (message.type == ROUTING_DATA) {
        return forwardMessage(message.destinationIp, frame, length);
    }
    // Se rechaza el paquete si su tipo no es conocido.
    return FRAME_INVALID;
}

// MOdulo 1, punto4 pedirle al Modulo 2 que guarde como llegar a la IP anunciada.
FrameRoutingResult registerAnnouncement(uint32_t announcedIp
, const RoutingLink *incomingLink) {
    
    // Se verifica que haya una IP anunciada y un vecino con IP y puerto.
    if (announcedIp == 0 || incomingLink == NULL ||
        incomingLink->nextHopIp == 0 || incomingLink->port == 0) {
        return FRAME_INVALID;
    }

    // TODO: pedir al Modulo 2 que guarde la IP y el enlace.
    // Acordar con los compañeros la funcion que se usara.
    return FRAME_MEMORY_ERROR; // Temporal: falta conectar la memoria.
}

// MOdulo1 punto5, buscar por donde enviar el mensaje y reenviarlo.
FrameRoutingResult forwardMessage(uint32_t destinationIp, const void *frame
, size_t length) {

    // Se verifica que haya un destino y un paquete con datos.
    if (destinationIp == 0 || frame == NULL || length == 0) {
        return FRAME_INVALID;
    }

    // TODO: consultar al Modulo 2 por donde llegar al destino.
    // Acordar la funcion para obtener la IP y puerto del vecino.

    // TODO usar esa respuesta para reenviar el paquete con syscallSendFrame.

    return FRAME_MEMORY_ERROR; // Temporalllllllllllllllllllllllllllllllllllllll
}
