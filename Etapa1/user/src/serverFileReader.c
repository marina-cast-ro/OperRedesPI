#include "serverFileReader.h"

// --- FUNCIONES RELACIONADAS LECTURA DE ARCHIVOS ---
// Inicializa la lectura del archivo 
FILE* openInputFile(const char *filePath) {
	FILE *file = fopen(filePath, "rb");	// rb: lectura binaria segura del archivo
	if (!file) {
		perror("Opening file failed");
	}
	return file;
}

// Lee el SIGUIENTE bloque de datos del archivo de texto.
// Retorna: Cantidad de bytes leídos reales (0 indica Fin de Archivo / EOF).
size_t readNextPayload(FILE *file, uint8_t *payloadBuffer) {
	if (!file || !payloadBuffer) return 0;

	// fread avanza automáticamente el puntero del archivo en cada llamada
    return fread(payloadBuffer, 1, MAX_PAYLOAD_SIZE, file);
}

// Cierra el archivo
void closeInputFile(FILE *file) {
    if (file) {
        fclose(file);
    }
}