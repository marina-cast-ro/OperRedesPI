#include "frameRouting.h"
#include "../pi-router/include/routingTable.h"

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

    // Se le pide al Modulo 2 que guarde en la memoria simulada que a announcedIp
    // se llega por el vecino que mando el anuncio.
    if (saveRoute(announcedIp, incomingLink->nextHopIp, incomingLink->port) != 0) {
        return FRAME_MEMORY_ERROR;
    }
    return FRAME_ROUTE_SAVED;
}

// MOdulo1 punto5, buscar por donde enviar el mensaje y reenviarlo.
FrameRoutingResult forwardMessage(uint32_t destinationIp, const void *frame
, size_t length) {

    // Se verifica que haya un destino y un paquete con datos.
    if (destinationIp == 0 || frame == NULL || length == 0) {
        return FRAME_INVALID;
    }

    // Se le pregunta al Modulo 2 por cual vecino se llega al destino.
    RoutingLink link = {0};
    if (findRoute(destinationIp, &link.nextHopIp, &link.port) != 0) {
        return FRAME_NO_ROUTE;
    }

    // TODO usar link.nextHopIp y link.port para reenviar el paquete con syscallSendFrame.

    return FRAME_SEND_ERROR; // Temporal: falta el envio.
}