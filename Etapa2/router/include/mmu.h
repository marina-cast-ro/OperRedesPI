#ifndef MMU_H
#define MMU_H

#define NUM_PAGES 32			// Cubre de sobra los ~20 hosts + enrutadores
#define TLB_SIZE 4 				// "Memoria caché" pequeña para guardar las traducciones usadas recientemente
#define MMU_ERROR 0xFFFFFFFF

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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
// Traduce una dirección virtual a física usando TLB y Page Table
// Retorna la dirección física traducida, o MMU_ERROR (0xFFFFFFFF) si hay Page Fault o VPN fuera de rango
uint32_t translateAddress(const uint32_t virtAddress);

// Inicializa la Page Table en 1:1 y la TLB totalmente vacía/inválida
void initMMU(void);

#endif // MMU_H