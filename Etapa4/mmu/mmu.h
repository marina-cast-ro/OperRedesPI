#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// Memoria simulada de la Etapa 2, ampliada para la Etapa 4:
// 128 paginas de 8 bytes = 1 KB (la tabla de costos de 32x32 celdas de 1 byte cabe justa).
// Direccion virtual de 10 bits = 7 bits de VPN + 3 bits de offset.
#define PAGE_OFFSET_BITS 3
#define BYTES_PER_PAGE   (1u << PAGE_OFFSET_BITS)   // 8
#define NUM_PAGES        128                        // DEBE ser potencia de 2 (se usa NUM_PAGES-1 como mascara)
#define MEMORY_SIZE      (NUM_PAGES * BYTES_PER_PAGE)
#define TLB_SIZE         4                          // "Memoria cache" pequena de traducciones recientes
#define MMU_ERROR        0xFFFFFFFF

// =======================================================
// PFN = Physical Frame Number, VPN = Virtual Page Number
// =======================================================

// Fila del Page Table
typedef struct {
	bool valid;		// Estado del mapeo en Page Table
	uint32_t pfn; 	// Numero de marco fisico correspondiente
} PageTableEntry;

// Fila del TLB ("Cache")
typedef struct {
	bool valid;		// Estado del mapeo en TLB
	uint32_t vpn;	// Numero de pagina a traducir
	uint32_t pfn; 	// Numero de marco fisico correspondiente
} TLBEntry;

// Una MMU completa. Ya no es un singleton: cada enrutador (cada tabla) tiene la suya,
// asi la simulacion de 5 islas en un mismo proceso no comparte memoria entre nodos.
// NO es thread-safe por si sola: translateAddress modifica la TLB incluso al LEER
// (en un TLB Miss), asi que todo acceso, lectura o escritura, debe hacerse con el
// candado del dueno tomado.
typedef struct {
	PageTableEntry pageTable[NUM_PAGES];	// indice = VPN
	TLBEntry tlb[TLB_SIZE];
	uint8_t tlbNextVictim;					// indice FIFO para reemplazar en la TLB
	uint8_t ram[MEMORY_SIZE];				// memoria fisica

	// Estadisticas que pide el enunciado (punto 4)
	uint32_t tlbHits;
	uint32_t tlbMisses;
	uint32_t pageFaults;
} mmu_t;

// Inicializa la Page Table en 1:1, la TLB vacia, la RAM en 0 y los contadores en 0
void initMMU(mmu_t *m);

// Traduce una direccion virtual a fisica usando TLB y Page Table.
// Retorna la direccion fisica, o MMU_ERROR si hay Page Fault o VPN fuera de rango
uint32_t translateAddress(mmu_t *m, uint32_t virtAddress);

// Lee/escribe un byte pasando por la traduccion. Retornan 0 si pudieron, -1 si la direccion no es valida
int mmuWriteByte(mmu_t *m, uint32_t virtAddress, uint8_t data);
int mmuReadByte(mmu_t *m, uint32_t virtAddress, uint8_t *outData);

// Pone los contadores en 0 (para no contar la carga inicial de la tabla)
void mmuResetStats(mmu_t *m);

// Imprime aciertos/fallos de TLB y fallos de pagina
void mmuImprimirEstadisticas(const mmu_t *m, int isla);

#endif // MMU_H
