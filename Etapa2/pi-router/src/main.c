#include "mmu.h"


// --- MAIN DE PRUEBAS PARA MMU ---
int main(void) {
	initMMU();

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