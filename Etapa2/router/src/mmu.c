#include "mmu.h"

// Arreglo estatico que representa la Page Table 
// indice = VNP, valor = PFN
static PageTableEntry pageTable[NUM_PAGES];

// Arreglo estatico que representa la TLB
static TLBEntry tlb[TLB_SIZE];
// Indice para utilizar FIFO en TLB (de acceso global)
// Permite reemplazar en la TLB la entrada mas antigua con la mas nueva
static uint8_t tlbNextVictim = 0; 

void updateTLB(void) {
	tlbNextVictim = (uint8_t)((tlbNextVictim + 1) % TLB_SIZE);
}

void initMMU(void) {
	// 1. Inicializar la Page Table con mapeo 1:1 válido
	for(size_t i = 0; i <  NUM_PAGES; i++) {
		pageTable[i].pfn = (uint8_t)i;
		pageTable[i].valid = true;
	}
	// 2. Limpiar la TLB (arranca completamente vacía/inválida)
	for(size_t i = 0; i < TLB_SIZE; i++) {
		tlb[i].vpn = 0;
		tlb[i].pfn = 0;
		tlb[i].valid = false;
	}
	// 3. Reiniciar el índice circular para el reemplazo FIFO
    tlbNextVictim = 0;
}

uint32_t translateAddress(const uint32_t virtAddress) {
	// 1. Extraer VPN y Offset
	// Desplazar los ultimos 3 bits a la derecha (offset) y quedarse con los 5 restantes (VPN)
	uint32_t vpn = (virtAddress >> 3) & 0x1F;	
	// Extraer offset de los ultimos 3 bits a la derecha (usando un AND bit a bit con el numero 7)
	uint32_t offset = virtAddress & 0x07;

	if (vpn >= NUM_PAGES) {
		printf("[MMU] Numero de pagina fuera de rango!\n");
        return MMU_ERROR; // Fuera de rango
    }

    // 2. Buscar en TLB
	for(size_t i = 0; i < TLB_SIZE; i++) {
		// --- TLB HIT ---
		if(tlb[i].valid && tlb[i].vpn == (uint8_t)vpn) {
			printf("[MMU] Hubo un TLB Hit en la dirección %zu del TLB!\n", i);
			return (tlb[i].pfn << 3) | offset ;
		}
	}
	
	// --- TLB MISS ---
    // 3. Buscar en Page Table
	if(pageTable[vpn].valid == true) {
		printf("[MMU] Hubo un TLB Miss! Trayendo traduccion del Page Table al TLB.\n");

		// Actualizar TLB con nueva traduccion (FIFO)
		tlb[tlbNextVictim].vpn = (uint8_t)vpn;
		tlb[tlbNextVictim].pfn = pageTable[vpn].pfn;
		tlb[tlbNextVictim].valid = true;
		updateTLB();

		return (pageTable[vpn].pfn << 3) | offset ;
	} else {
		// Page fault
		printf("[MMU] Page Fault! El numero de pagina es invalido.\n");
		return MMU_ERROR;
	}
}

void removeRouteBySocket(uint32_t socketFd) {
    // 1. Limpiar en la Page Table
    for (size_t i = 0; i < NUM_PAGES; i++) {
        if (pageTable[i].valid && pageTable[i].pfn == (uint8_t)socketFd) {
            pageTable[i].valid = false;
            printf("[MMU] Pagina %zu invalidada en Page Table para socket %u\n", i, socketFd);
        }
    }

    // 2. Limpiar en la TLB (si estaba en caché)
    for (size_t i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].pfn == (uint8_t)socketFd) {
            tlb[i].valid = false;
            printf("[MMU] Entrada %zu invalidada en TLB para socket %u\n", i, socketFd);
        }
    }
}