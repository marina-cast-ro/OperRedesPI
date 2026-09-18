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

uint32_t translateAddress(const uint32_t virt_address) {
    // Extraer VPN (bits superiores) y Offset (3 bits inferiores)

}