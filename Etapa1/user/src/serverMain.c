#include "serverSender.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <ip> <puerto> <ruta_txt>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *path = argv[3];

    if (sendFileOverProtocol(path, ip, port) == 0) {
        printf("[USER SPACE] Envio exitoso\n\n");
		printf("--------------------------------------------------\n\n");
		FILE *reset = fopen(path, "w");//el archivo ahora esta vacio
    	if(reset){
			fclose(reset);
		}
		return 0;
    } else {
        fprintf(stderr, "[USER SPACE] Fallo el envio\n\n");
		printf("--------------------------------------------------\n\n");
        return 1;
    }
}