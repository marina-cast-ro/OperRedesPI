#include "tlbPaging.h"

// Arreglo estatico que representa la Page Table 
// indice = VNP, valor = PFN
static PageTableEntry pageTable[NUM_PAGES];
//static TLBEntry tlb[TLB_SIZE];

uint32_t translate_address(const uint32_t virt_address) {
    // Extraer VPN (bits superiores) y Offset (3 bits inferiores)

}