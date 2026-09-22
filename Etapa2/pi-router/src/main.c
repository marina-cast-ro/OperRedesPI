#include "mmu.h"
#include "router.h"

// Esto se corre con:
//      ~/OperRedesPI/Etapa2/pi-router/src
//      gcc main.c mmu.c router.c virtualMemory.c -I../include -o p
//      ./p

// --- MAIN DE PRUEBAS PARA ROUTER + MMU ---
int main(void) {
	initMMU();
    
    // Activa el router para la escucha y mensaje inicial hacia nodos vecinos
    pthread_t router_thread = activateRouter();

	printf("\n--- 1. Llenando la TLB (Páginas 0, 1, 2, 3) ---\n");
    translateAddress(0); // VPN 0 -> Miss (TLB[0])
    translateAddress(1); // VPN 1 -> Miss (TLB[1])
    translateAddress(2); // VPN 2 -> Miss (TLB[2])
    translateAddress(3); // VPN 3 -> Miss (TLB[3])

    printf("\n--- 2. Provocando Reemplazo FIFO (Página 4 reemplaza a la 0) ---\n");
    translateAddress(4); // VPN 4 -> Miss (Reemplaza en TLB[0])

    printf("\n--- 3. Verificando que la Página 0 ya no está en TLB ---\n");
    translateAddress(0); // VPN 0 -> Debería ser Miss de nuevo
	
    // Mantiene vivo al hilo de escucha del router
    pthread_join(router_thread, NULL);

	return 0;
} 