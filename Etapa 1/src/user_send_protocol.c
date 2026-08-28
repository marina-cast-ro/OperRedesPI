
#include "user_send_protocol.h"

void readFileInBlocks(const char *filePath) {
	FILE *file = fopen(filePath, "rb");		// rb: lectura binaria segura del archivo
	if (!file) {
		perror("Error al abrir el archivo");
		return;
	}
	
	uint8_t buffer[MAX_PAYLOAD_SIZE];
	size_t readBytes;

	// fread lee hasta BLOCK_SIZE elementos de 1 byte
	while ((readBytes = fread(buffer, 1, MAX_PAYLOAD_SIZE, file)) > 0) {
		printf("Read block: %zu bytes.\n", readBytes);
	}
	fclose(file);
}

int main (int argc, char *argv[]) {	
	readFileInBlocks("tests.txt");
	return 0;
}