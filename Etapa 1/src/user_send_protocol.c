
#include "user_send_protocol.h"


// Inicializa la lectura del archivo 
FILE* openInputFile(const char *filePath) {
	FILE *file = fopen(filePath, "rb");	// rb: lectura binaria segura del archivo
	if (!file) {
		perror("Opening file failed");
	}
	return file;
}

// Lee el SIGUIENTE bloque de datos del disco.
// Retorna: Cantidad de bytes leídos reales (0 indica Fin de Archivo / EOF).
int readNextPayload(FILE *file, uint8_t *payload_buffer) {
	if (!file || !payload_buffer) return 0;

	// fread avanza automáticamente el puntero del archivo en cada llamada
    return fread(payload_buffer, 1, MAX_PAYLOAD_SIZE, file);
}

// Cierra el archivo
void close_input_file(FILE *file) {
    if (file) {
        fclose(file);
    }
}

int main (int argc, char *argv[]) {	
	FILE* file = openInputFile("tests.txt");

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	size_t payloadSize;

	while((payloadSize = readNextPayload(file, buffer)) > 0) {
		printf("Read bytes: %zu\n", payloadSize);
	}	

	printf("There's no more bytes to read!\n");

	close_input_file(file);
	return 0;
}