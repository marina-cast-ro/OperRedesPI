#include "tlbPaging.h"

// Arreglo estatico que representa la Page Table 
// indice = VNP, valor = PFN
static PageTableEntry pageTable[NUM_PAGES];

// Arreglo estatico que representa la TLB
static TLBEntry tlb[TLB_SIZE];
// Indice para utilizar FIFO en TLB (de acceso global)
// Permite reemplazar en la TLB la entrada mas antigua con la mas nueva
static uint8_t tlbNextVictim = 0; 

void updateTLB() {
	tlbNextVictim = (tlbNextVictim + 1) % TLB_SIZE;
}

uint32_t translateAddress(const uint32_t virtAddress) {
	// 1. Extraer VPN y Offset
	// Desplazar los ultimos 3 bits a la derecha (offset) y quedarse con los 5 restantes (VPN)
	uint32_t vpn = virtAddress >> 3;	
	// Extraer offset de los ultimos 3 bits a la derecha (usando un AND bit a bit con el numero 7)
	uint32_t offset = virtAddress & 0x07;

	if (vpn >= NUM_PAGES) {
		printf("[MMU] Numero de pagina fuera de rango!");
        return MMU_ERROR; // Fuera de rango
    }

    // 2. Buscar en TLB
	for(size_t i = 0; i < TLB_SIZE; i++) {
		// --- TLB HIT ---
		if(tlb[i].vpn == vpn && tlb[i].valid == true) {
			printf("[MMU] Hubo un TLB Hit en la dirección %zu del TLB!", i);
			return (tlb[i].pfn << 3) | (offset & 0x07);
		}
	}
	
	// --- TLB MISS ---
    // 3. Buscar en Page Table
	if(pageTable[vpn].valid == true) {
		printf("[MMU] Hubo un TLB Miss! Trayendo traduccion del Page Table al TLB.");

		// Actualizar TLB con nueva traduccion (FIFO)
		tlb[tlbNextVictim].valid = true;
		tlb[tlbNextVictim].vpn = vpn;
		tlb[tlbNextVictim].pfn = pageTable[vpn].pfn;
		updateTLB();

		return (pageTable[vpn].pfn << 3) | (offset & 0x07);
	} else {
		// Page fault
		printf("[MMU] Page Fault! El numero de pagina es invalido.");
		return MMU_ERROR;
	}
}