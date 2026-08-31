#include "../include/stopAndWait.h"

// Bit de secuencia del emisor. Alterna entre 0 y 1 con cada trama confirmada.
// Es lo que permite al receptor detectar tramas duplicadas y descartarlas.
static uint8_t currentSeq;

// Deja el protocolo listo para una transferencia nueva.
// Se llama una vez antes de enviar la primera trama.
void initProtocolState(void) {
    currentSeq = 0;
}