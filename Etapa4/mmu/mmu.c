#include "mmu.h"
#include <string.h>

// Los printf por acceso quedan apagados (inundan la consola y se imprimirian con el candado tomado).
// Para depurar: gcc -DMMU_DEBUG ...
#ifdef MMU_DEBUG
#define MMU_LOG(...) printf(__VA_ARGS__)
#else
#define MMU_LOG(...) ((void)0)
#endif

void initMMU(mmu_t *m) {
	// 1. Page Table con mapeo 1:1 valido
	for (size_t i = 0; i < NUM_PAGES; i++) {
		m->pageTable[i].pfn = (uint32_t)i;
		m->pageTable[i].valid = true;
	}
	// 2. TLB arranca completamente vacia/invalida
	for (size_t i = 0; i < TLB_SIZE; i++) {
		m->tlb[i].vpn = 0;
		m->tlb[i].pfn = 0;
		m->tlb[i].valid = false;
	}
	// 3. Indice circular del reemplazo FIFO
	m->tlbNextVictim = 0;
	// 4. RAM y contadores
	memset(m->ram, 0, sizeof(m->ram));
	mmuResetStats(m);
}

void mmuResetStats(mmu_t *m) {
	m->tlbHits = 0;
	m->tlbMisses = 0;
	m->pageFaults = 0;
}

uint32_t translateAddress(mmu_t *m, uint32_t virtAddress) {
	// Los ultimos 3 bits son el offset y los 7 restantes la VPN
	uint32_t vpn = (virtAddress >> PAGE_OFFSET_BITS) & (NUM_PAGES - 1);
	uint32_t offset = virtAddress & (BYTES_PER_PAGE - 1);

	// --- TLB HIT ---
	for (size_t i = 0; i < TLB_SIZE; i++) {
		if (m->tlb[i].valid && m->tlb[i].vpn == vpn) {
			m->tlbHits++;
			MMU_LOG("[MMU] TLB Hit en la entrada %zu\n", i);
			return (m->tlb[i].pfn << PAGE_OFFSET_BITS) | offset;
		}
	}

	// --- TLB MISS: buscar en la Page Table ---
	m->tlbMisses++;
	if (m->pageTable[vpn].valid) {
		MMU_LOG("[MMU] TLB Miss, trayendo VPN %u de la Page Table\n", vpn);

		// Actualizar la TLB (FIFO)
		m->tlb[m->tlbNextVictim].vpn = vpn;
		m->tlb[m->tlbNextVictim].pfn = m->pageTable[vpn].pfn;
		m->tlb[m->tlbNextVictim].valid = true;
		m->tlbNextVictim = (uint8_t)((m->tlbNextVictim + 1) % TLB_SIZE);

		return (m->pageTable[vpn].pfn << PAGE_OFFSET_BITS) | offset;
	}

	// --- Page fault ---
	m->pageFaults++;
	MMU_LOG("[MMU] Page Fault en VPN %u\n", vpn);
	return MMU_ERROR;
}

int mmuWriteByte(mmu_t *m, uint32_t virtAddress, uint8_t data) {
	// translateAddress solo mira 7 bits de pagina: una direccion >= MEMORY_SIZE "daria la vuelta"
	// y caeria sobre otra pagina. Por eso el rango se valida aqui y se cuenta como fallo de pagina.
	if (virtAddress >= MEMORY_SIZE) {
		m->pageFaults++;
		return -1;
	}
	uint32_t phys = translateAddress(m, virtAddress);
	if (phys == MMU_ERROR) return -1;
	m->ram[phys] = data;
	return 0;
}

int mmuReadByte(mmu_t *m, uint32_t virtAddress, uint8_t *outData) {
	if (outData == NULL) return -1;
	if (virtAddress >= MEMORY_SIZE) {
		m->pageFaults++;
		return -1;
	}
	uint32_t phys = translateAddress(m, virtAddress);
	if (phys == MMU_ERROR) return -1;
	*outData = m->ram[phys];
	return 0;
}

void mmuImprimirEstadisticas(const mmu_t *m, int isla) {
	uint32_t total = m->tlbHits + m->tlbMisses;
	printf("[MMU isla %d] TLB aciertos=%u, TLB fallos=%u (%u accesos, %.1f%% de aciertos) | fallos de pagina=%u\n",
	       isla, m->tlbHits, m->tlbMisses, total,
	       total ? 100.0 * m->tlbHits / total : 0.0, m->pageFaults);
}
