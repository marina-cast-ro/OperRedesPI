#include <stdio.h>
#include <string.h>
#include "../include/mmu.h"
#include "../include/virtualMemory.h"
#include "../include/configParser.h"
#include "../include/router.h"
#include "../include/forwarding.h"
#include "../../host/include/listener.h"
#include "../../host/include/sender.h"

// --- MAIN DE PRUEBAS PARA MMU ---
int test_main(void) {
	initMMU();
    
    // 1. Inicializar la memoria física y virtual ANTES de cualquier cosa
	initMMU();
	virtualMemoryInit();

	// 2. Leer config y precargar interfaces virtuales en la MMU
	ConfigRouter routerConfig = parseConfigAndPreload("config.txt");

	// 3. Levantar la red: Inicia el hilo que hace accept() y recibe datos
	if (initRouterListen(routerConfig) == ERROR_ROUTER) {
		return -1;
	}

	printf("--- 1. Llenando la TLB (Páginas 0, 1, 2, 3) ---\n");
    translateAddress(0); // VPN 0 -> Miss (TLB[0])
    translateAddress(1); // VPN 1 -> Miss (TLB[1])
    translateAddress(2); // VPN 2 -> Miss (TLB[2])
    translateAddress(3); // VPN 3 -> Miss (TLB[3])

    printf("\n--- 2. Provocando Reemplazo FIFO (Página 4 reemplaza a la 0) ---\n");
    translateAddress(4); // VPN 4 -> Miss (Reemplaza en TLB[0])

    printf("\n--- 3. Verificando que la Página 0 ya no está en TLB ---\n");
    translateAddress(0); // VPN 0 -> Debería ser Miss de nuevo
	
	return 0;
} 