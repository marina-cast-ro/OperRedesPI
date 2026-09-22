#include "../include/protocol.h"

#include <arpa/inet.h>
#include <string.h>

int decodeFrame(const void *frame, size_t length, RoutingMessage *message) {

    // Se revisa que haya un paquete y un lugar donde guardarlo
    if (frame == NULL || length == 0 || message == NULL) {
        return -1;
    }

    // Se revisan bytes nulos y saltos de linea dentro del mensaje de texto
    // para no permitirlos dentro del mensaje
    //TODO ver si lo dejamos asi o que si los acepte
    const char *bytes = frame;
    if (memchr(bytes, '\0', length) != NULL ||
        memchr(bytes, '\r', length) != NULL ||
        memchr(bytes, '\n', length) != NULL) {
        return -1;
    }

    // Se revisa que haya al menos un separador('|') en el mensaje
    const char *separator = memchr(bytes, PROTOCOL_SEPARATOR, length);
    if (separator == NULL) {
        return -1;
    }

    // Reconocer el tipo de mensaje
    RoutingMessage decoded = {0};
    size_t typeLength = (size_t)(separator - bytes); //Calcula longitud de la palabra

    // Con cada if comparamos que la longitud de la palabra sea igual a una de los 3 tipos que tenemos
    // Y tambien comparamos byte a byte para que el contenido sea el mismo
    // Si se cumplen las condiciones se le asigna tipo segun el mensaje que sea
    if (typeLength == sizeof(PROTOCOL_ANNOUNCE) - 1 
    && memcmp(bytes, PROTOCOL_ANNOUNCE, typeLength) == 0) {
        decoded.type = ROUTING_ANNOUNCEMENT;

    } else if (typeLength == sizeof(PROTOCOL_ADVERTISE) - 1 
    && memcmp(bytes, PROTOCOL_ADVERTISE, typeLength) == 0) {
        decoded.type = ROUTING_ADVERTISEMENT;

    } else if (typeLength == sizeof(PROTOCOL_DATA) - 1 
    && memcmp(bytes, PROTOCOL_DATA, typeLength) == 0) {
        decoded.type = ROUTING_DATA;
    } else {
        return -1;
    }

    // GUARDAR LA IP ============================================
    // En los 3 tipos de mensaje despues del nombre esta la IP
    const char *ipStart = separator + 1;
    const char *end = bytes + length;

    // Solo los mensajes de tipo DATA traen otro separador (|)
    // Buscamos su posicion en caso de que se encuentre para guardarla
    // Si no se encuentra se queda en NULL el puntero
    const char *dataSeparator = memchr(ipStart, PROTOCOL_SEPARATOR, (size_t)(end - ipStart));

    if (decoded.type == ROUTING_DATA) {
        if (dataSeparator == NULL) {
            return -1;
        }
        decoded.data = dataSeparator + 1;
        decoded.dataLength = (size_t)(end - decoded.data);
    } else if (dataSeparator != NULL) { 
        // Entra aqui porque son de tipo ANNOUNCE & ADVERTISE y dataSeparator == NULL
        return -1;
    }

    //Si dataSeparator existe (DATA), el fin de la IP es el separador (|)
    //Si dataSeparator es NULL  (ANNOUNCE o ADVERTISE), el fin de la IP es el final de todo el mensaje (end).
    const char *ipEnd = dataSeparator != NULL ? dataSeparator : end;

    size_t ipLength = (size_t)(ipEnd - ipStart);
    char ipText[INET_ADDRSTRLEN];
    if (ipLength == 0 || ipLength >= sizeof(ipText)) {
        return -1;
    }
    memcpy(ipText, ipStart, ipLength);
    ipText[ipLength] = '\0'; // Se agreaga el caracter nulo al final de la IP

    // Transforma el string de la IP y verifica que sea valida y no sea 0.0.0.0
    struct in_addr address;
    if (inet_pton(AF_INET, ipText, &address) != 1 || address.s_addr == 0) {
        return -1;
    }

    // Se guarda la IP en el campo correspondiente al tipo de mensaje.
    if (decoded.type == ROUTING_DATA) {
        decoded.destinationIp = address.s_addr;
    } else {
        decoded.announcedIp = address.s_addr;
    }

    // Se entrega el resultado solo si el el mensaje es valido.
    *message = decoded;
    return 0;
}
