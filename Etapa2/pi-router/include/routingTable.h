#ifndef ROUTINGTABLE_H
#define ROUTINGTABLE_H

#include "virtualMemory.h"

// Guarda en la memoria virtual la ruta hacia destinationIp: para llegar ahí, el mensaje
// se reenvía por la interfaz interfaceId. Si destinationIp ya tenía ruta, la reemplaza.
// Retorna 0 si la guardó, -1 si destinationIp es 0 o la tabla está llena
int saveRoute(uint32_t destinationIp, uint32_t interfaceId);

// Busca la ruta hacia destinationIp leyendo la tabla byte a byte.
// Retorna 0 y deja la interfaz en *outInterfaceId si la encontró, -1 si no hay ruta
int findRoute(uint32_t destinationIp, uint32_t *outInterfaceId);

#endif // ROUTINGTABLE_H