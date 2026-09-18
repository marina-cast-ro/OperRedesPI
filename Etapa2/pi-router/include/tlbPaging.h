#ifndef TLBPAGING_H
#define TLBPAGING_H

#define NUM_PAGES 32	// Cubre de sobra los ~20 hosts + enrutadores
#define TLB_SIZE 4 		// "Memoria caché" pequeña para guardar las traducciones usadas recientemente

#include <stdint.h>
#include <stdbool.h>
// =======================================================
// PFN = Physical Frame Number, VPN = Virtual Page Number
// =======================================================

// Struct que representa cada entrada o fila del Page Table
typedef struct {
	bool valid;		// Estado del mapeo en Page Table
	uint32_t pfn; 	// Número de marco físico correspondiente
} PageTableEntry;

// Struct que representa cada entrada o fila del TLB ("Caché")
typedef struct {
	bool valid;		// Estado del mapeo en TLB
	uint32_t vpn;	// Número de página a traducir
	uint32_t pfn; 	// Número de marco físico correspondiente
} TLBEntry;

// --- Firmas de funciones ---
uint32_t translateAddress(const uint32_t virt_address);

#endif // TLBPAGING_H