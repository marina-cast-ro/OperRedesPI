#include "frameRouting.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ip(const char *text) {
    struct in_addr address;
    assert(inet_pton(AF_INET, text, &address) == 1);
    return address.s_addr;
}

static void checkAnnouncements(void) {
    RoutingMessage message = {0};
    const char *announce = "ANNOUNCE|192.168.1.101";
    assert(decodeFrame(announce, strlen(announce), &message) == 0);
    assert(message.type == ROUTING_ANNOUNCEMENT);
    assert(message.announcedIp == ip("192.168.1.101"));
    assert(message.destinationIp == 0);
    assert(message.data == NULL && message.dataLength == 0);

    const char *advertise = "ADVERTISE|10.2.0.2";
    assert(decodeFrame(advertise, strlen(advertise), &message) == 0);
    assert(message.type == ROUTING_ADVERTISEMENT);
    assert(message.announcedIp == ip("10.2.0.2"));
    assert(message.destinationIp == 0);
    assert(message.data == NULL && message.dataLength == 0);
}

static void checkData(void) {
    RoutingMessage message = {0};
    const char *frame = "DATA|10.2.0.2|Hola|companeros";
    assert(decodeFrame(frame, strlen(frame), &message) == 0);
    assert(message.type == ROUTING_DATA);
    assert(message.destinationIp == ip("10.2.0.2"));
    assert(message.announcedIp == 0);
    assert(message.data == frame + strlen("DATA|10.2.0.2|"));
    assert(message.dataLength == strlen("Hola|companeros"));
    assert(memcmp(message.data, "Hola|companeros", message.dataLength) == 0);

    const char *empty = "DATA|10.2.0.2|";
    assert(decodeFrame(empty, strlen(empty), &message) == 0);
    assert(message.dataLength == 0);

    // El resultado anterior no debe dejar datos en un anuncio nuevo.
    const char *announce = "ANNOUNCE|10.1.0.2";
    assert(decodeFrame(announce, strlen(announce), &message) == 0);
    assert(message.data == NULL && message.dataLength == 0);
    assert(message.destinationIp == 0);
}

static void checkLengths(void) {
    RoutingMessage message = {0};
    // Este arreglo tiene exactamente el mensaje, sin un byte nulo final.
    const char frame[sizeof("DATA|10.2.0.2|Hola") - 1] = "DATA|10.2.0.2|Hola";
    assert(decodeFrame(frame, sizeof(frame), &message) == 0);
    assert(message.dataLength == 4);
    assert(memcmp(message.data, "Hola", 4) == 0);

    // No se debe leer el resto del buffer cuando length termina antes.
    const char *buffer = "ANNOUNCE|10.1.0.2|sobrante";
    assert(decodeFrame(buffer, strlen("ANNOUNCE|10.1.0.2"), &message) == 0);
    assert(message.announcedIp == ip("10.1.0.2"));

    const char embeddedNull[] = "DATA|10.2.0.2|Hola\0resto";
    assert(decodeFrame(embeddedNull, sizeof(embeddedNull) - 1, &message) == -1);
    const char *withNull = "ANNOUNCE|10.1.0.2";
    assert(decodeFrame(withNull, strlen(withNull) + 1, &message) == -1);
}

static void checkInvalidMessages(void) {
    const char *invalid[] = {
        "", "ANNOUNCE", "ANNOUNCE|", "ADVERTISE|", "DATA|10.2.0.2",
        "DATA||Hola", "HELLO|10.1.0.2", "announce|10.1.0.2",
        "[ANNOUNCE|10.1.0.2]", "ANNOUNCE|10.1.0.2|extra",
        "ADVERTISE|10.1.0.2|", "ANNOUNCE|256.1.1.1",
        "ANNOUNCE|192.168.1", "ANNOUNCE|1.2.3.4.5", "ANNOUNCE|::1",
        "ANNOUNCE| 10.1.0.2", "ANNOUNCE|10.1.0.2 ", "ANNOUNCE|0.0.0.0",
        "DATA|0.0.0.0|Hola", "ANNOUNCE|1111.1111.1111.1111",
        "ANNOUNCE|10.1.0.2\n", "DATA|10.2.0.2|Hola\nresto",
        "DATA|10.2.0.2|Hola\rresto", "|10.1.0.2"
    };
    const RoutingMessage original = {
        .type = ROUTING_DATA, .announcedIp = 123, .destinationIp = 456,
        .data = "sin cambiar", .dataLength = 11
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        RoutingMessage message = original;
        assert(decodeFrame(invalid[i], strlen(invalid[i]), &message) == -1);
        // Un error no debe entregar un resultado parcialmente leido.
        assert(message.type == original.type);
        assert(message.announcedIp == original.announcedIp);
        assert(message.destinationIp == original.destinationIp);
        assert(message.data == original.data);
        assert(message.dataLength == original.dataLength);
    }
    RoutingMessage message = {0};
    assert(decodeFrame(NULL, 1, &message) == -1);
    assert(decodeFrame("ANNOUNCE|10.1.0.2", 17, NULL) == -1);
}

static void checkRoutingIntegration(void) {
    RoutingLink neighbor = {ip("192.168.1.101"), 5000, 1};
    const char *frames[] = {
        "ANNOUNCE|192.168.1.101", "ADVERTISE|10.1.0.2", "DATA|10.2.0.2|Hola"
    };
    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); ++i) {
        // Los tres se leen bien y llegan a la integracion pendiente de memoria.
        assert(processReceivedFrame(frames[i], strlen(frames[i]), &neighbor)
               == FRAME_MEMORY_ERROR);
    }
    assert(processReceivedFrame(frames[0], strlen(frames[0]), NULL)
           == FRAME_INVALID);
    assert(processReceivedFrame(frames[2], strlen(frames[2]), NULL)
           == FRAME_MEMORY_ERROR);
    const char *invalid = "DATA|no-es-IP|Hola";
    assert(processReceivedFrame(invalid, strlen(invalid), &neighbor) == FRAME_INVALID);
}

int main(void) {
    checkAnnouncements();
    checkData();
    checkLengths();
    checkInvalidMessages();
    checkRoutingIntegration();
    puts("OK: protocolo e integracion con frameRouting.");
    return 0;
}
