#include "../include/protocol.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

int decodeFrame(const void *frame, size_t length, RoutingMessage *message) {
    // Se revisa que haya un paquete y un lugar donde guardarlo
    if (frame == NULL || length == 0 || message == NULL) {
        return -1;
    }

    const char *bytes = (const char *)frame;

    // Se limpia los caracteres (\n, \r, \0):
    const char *nullByte = memchr(bytes, '\0', length);
    if (nullByte != NULL) {
        length = (size_t)(nullByte - bytes);
    }
    while (length > 0 && (*bytes == '\r' || *bytes == '\n')) {
        bytes++;
        length--;
    }

    while (length > 0 && (bytes[length - 1] == '\r' || bytes[length - 1] == '\n')) {
        length--;
    }

    // Si no queda contenido despues de la limpieza de los caracteres la trama no es valida
    if (length == 0) {
        return -1;
    }

    // Se revisa que haya al menos un separador ('|') en el mensaje
    const char *separator = memchr(bytes, PROTOCOL_SEPARATOR, length);
    if (separator == NULL) {
        return -1;
    }

    // Reconocer el tipo de mensaje
    RoutingMessage decoded = {0};
    size_t typeLength = (size_t)(separator - bytes); // Calcula longitud de la palabra

    // Con cada if comparamos que la longitud de la palabra sea igual a una de los 3 tipos que tenemos
    // Y también comparamos byte a byte para que el contenido sea el mismo
    // Si se cumplen las condiciones se le asigna tipo según el mensaje que sea
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
    // En los 3 tipos de mensaje después del nombre está la IP
    const char *ipStart = separator + 1;
    const char *end = bytes + length;

    // Solo los mensajes de tipo DATA traen otro separador (|)
    // Buscamos su posición en caso de que se encuentre para guardarla
    const char *dataSeparator = memchr(ipStart, PROTOCOL_SEPARATOR, (size_t)(end - ipStart));

    if (decoded.type == ROUTING_DATA) {
        if (dataSeparator == NULL) {
            return -1;
        }
        decoded.data = dataSeparator + 1;
        decoded.dataLength = (size_t)(end - decoded.data);
    } else if (dataSeparator != NULL) { 
        // Entra aquí porque son de tipo ANNOUNCE o ADVERTISE y traen un separador extra no permitido
        return -1;
    }

    // Si dataSeparator existe (DATA), el fin de la IP es el separador (|)
    // Si dataSeparator es NULL (ANNOUNCE o ADVERTISE), el fin de la IP es el final del mensaje (end)
    const char *ipEnd = dataSeparator != NULL ? dataSeparator : end;

    size_t ipLength = (size_t)(ipEnd - ipStart);
    char ipText[INET_ADDRSTRLEN];
    if (ipLength == 0 || ipLength >= sizeof(ipText)) {
        return -1;
    }
    memcpy(ipText, ipStart, ipLength);
    ipText[ipLength] = '\0'; // Se agrega el caracter nulo al final de la IP

    // Transforma el string de la IP y verifica que sea válida y no sea 0.0.0.0
    // Manejo de IPs en binario de red exclusivamente con inet_pton (sin htonl/ntohl)
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

    // Se entrega el resultado solo si el mensaje es completamente válido.
    *message = decoded;
    return 0;
}

int encodeFrame(const RoutingMessage *message, char *buffer, size_t bufferSize) {
    if (message == NULL || buffer == NULL || bufferSize == 0) {
        return -1;
    }

    const char *typeStr = NULL;
    uint32_t ipBinary = 0;
    int isData = 0;

    switch (message->type) {
        case ROUTING_ANNOUNCEMENT:
            typeStr = PROTOCOL_ANNOUNCE;
            ipBinary = message->announcedIp;
            break;
        case ROUTING_ADVERTISEMENT:
            typeStr = PROTOCOL_ADVERTISE;
            ipBinary = message->announcedIp;
            break;
        case ROUTING_DATA:
            typeStr = PROTOCOL_DATA;
            ipBinary = message->destinationIp;
            isData = 1;
            break;
        default:
            return -1;
    }

    if (ipBinary == 0) {
        return -1;
    }

    struct in_addr address;
    address.s_addr = ipBinary;
    char ipText[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &address, ipText, sizeof(ipText)) == NULL) {
        return -1;
    }

    // Casos ANNOUNCE / ADVERTISE
    if (!isData) {
        int written = snprintf(buffer, bufferSize, "%s%c%s\n", typeStr, PROTOCOL_SEPARATOR, ipText);
        if (written < 0 || (size_t)written >= bufferSize) {
            return -1;
        }
        return written; // 
    }

    // Caso DATA
    size_t dataLen = 0;
    if (message->data != NULL) {
        dataLen = (message->dataLength > 0) ? message->dataLength : strlen(message->data);
    }

    size_t typeLen = strlen(typeStr);
    size_t ipLen = strlen(ipText);
    size_t prefixLen = typeLen + 1 + ipLen + 1; // "DATA|IP|"
    size_t totalBytes = prefixLen + dataLen + 1; // + '\n'
    size_t totalNeeded = totalBytes + 1; // + '\0'

    if (totalNeeded > bufferSize) {
        return -1;
    }

    int prefixWritten = snprintf(buffer, bufferSize, "%s%c%s%c", typeStr, PROTOCOL_SEPARATOR, ipText, PROTOCOL_SEPARATOR);
    if (prefixWritten < 0 || (size_t)prefixWritten != prefixLen) {
        return -1;
    }

    if (dataLen > 0 && message->data != NULL) {
        memcpy(buffer + prefixLen, message->data, dataLen);
    }

    buffer[prefixLen + dataLen] = '\n';
    buffer[prefixLen + dataLen + 1] = '\0';

    return (int)totalBytes; 
}