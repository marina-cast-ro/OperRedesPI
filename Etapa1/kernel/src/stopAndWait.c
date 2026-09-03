#include "../include/stopAndWait.h"
#include "../include/kernelSocket.h"
#include <linux/random.h>

// Configuración del temporizador de retransmisión
#define ACK_TIMEOUT_MS 200   // Espera del ACK antes de reenviar la trama
#define MAX_RETRIES    5     // Intentos por trama antes de darse por vencido

// Bit de secuencia del emisor. Alterna entre 0 y 1 con cada trama confirmada
// Es lo que permite al receptor detectar tramas duplicadas y descartarlas
static uint8_t currentSeq;

// Deja el protocolo listo para una transferencia nueva
// Se llama una vez antes de enviar la primera trama
void initProtocolState(void) {
    currentSeq = 0;
}

// Verifica si lo recibido es el ACK que estamos esperando
// Retorna 1 si es válido, 0 si no lo es
static int isValidAck(const uint8_t *buffer, int length, uint8_t expectedSeq) {
    const Header *header;

    if (length < (int)sizeof(Header)) return 0;

    header = (const Header *)buffer;
    return (header->type == PROTOCOL_FRAME_ACK) && (header->seqNumber == expectedSeq);
}

// Inyecta el seqNumber actual en la trama, la envía por UDP y espera el ACK
// Si el temporizador vence sin respuesta, reenvía la misma trama
// Retorna 0 si el receptor confirmó, o un código de error negativo
int sendFrameStopAndWait(const char *ip_dest, int port, const uint8_t *frameData, size_t length) {
    struct socket *socket = NULL;
    uint8_t frame[MAX_BUFFER_SIZE];
    uint8_t ackBuffer[sizeof(Header)];
    uint8_t expectedSeq;
    int attempt;
    int result;

    // Validación de parámetros, retorna EINVAL
    if (!ip_dest || !frameData || length == 0 || length > MAX_BUFFER_SIZE)
        return -EINVAL;

    // Se copia la trama porque frameData es const y hay que inyectarle el bit de secuencia actual antes de enviarla
    memcpy(frame, frameData, length);
    ((Header *)frame)->seqNumber = currentSeq;

    // El receptor confirma con el bit contrario, que además es el que corresponde usar en la trama siguiente
    expectedSeq = 1 - currentSeq;

    result = ksocketCreate(&socket);
    if (result < 0) return result;

    // Cada vuelta del ciclo es un envío de la misma trama
    for (attempt = 1; attempt <= MAX_RETRIES; attempt++) {

		// --- SIMULACIÓN DE PÉRDIDA DEL 30% ---
        if ((get_random_u32() % 100) < 30) {
            pr_warn("[KERNEL SPACE] [SIMULACION DE PERDIDA]: Trama descartada intencionalmente en intento %d\n", attempt);
            // Omitimos ksocket_sendto para que el paquete se "pierda"
            // Esto causa que recvfrom expire por timeout (-EAGAIN) y fuerce el reenvío
        } else {
			pr_info("[sendFrameStopAndWait] Enviando trama Seq: %d (Intento %d)...\n", currentSeq, attempt);
            result = ksocket_sendto(socket, ip_dest, port, frame, length);
            if (result < 0) {
                ksocketRelease(socket);
                return result;
            }
        }

        // ksocket_recvfrom aplica el temporizador internamente con sk_rcvtimeo
        result = ksocket_recvfrom(socket, ackBuffer, sizeof(ackBuffer), ACK_TIMEOUT_MS);

        if (result == -EAGAIN) {
            // Venció el temporizador: nadie respondió. Se reenvía la trama
            pr_warn("[KERNEL SPACE]: Timeout en intento %d, reenviando\n", attempt);
            continue;
        }

        if (result < 0) {
            // Error real del socket
            ksocketRelease(socket);
            return result;
        }

        if (isValidAck(ackBuffer, result, expectedSeq)) {
			pr_info("[KERNEL SPACE]: ACK recibido con exito, trama confirmada (Seq: %d)\n\n", expectedSeq);
            // El bit vuelve a 0 para que la proxima transferencia arranque en fase con el receptor.
            if (((Header *)frame)->type == PROTOCOL_FRAME_END) {
                initProtocolState();
            } else {
                currentSeq = expectedSeq;
            }
            ksocketRelease(socket);
            return 0;
        }

        // Llegó algo que no es el ACK esperado, ya sea un duplicado o basura
        // Se descarta y el siguiente intento reenvía la trama
        pr_warn("[KERNEL SPACE]: Respuesta invalida en intento %d\n", attempt);
    }

    pr_err("[KERNEL SPACE]: Sin ACK tras %d intentos\n\n", MAX_RETRIES);
    ksocketRelease(socket);
    return -ETIMEDOUT;
}