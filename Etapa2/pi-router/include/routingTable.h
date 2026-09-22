#ifndef ROUTINGTABLE_H
#define ROUTINGTABLE_H

#include "virtualMemory.h"

// Guarda en la memoria virtual la ruta hacia destinationIp asociada a la interfaz indicada.
// Si destinationIp ya tenía ruta, la reemplaza.
// Retorna 0 si la guardó, -1 si destinationIp es 0, interfaz es NULL o la tabla está llena.
int saveRoute(uint32_t destinationIp, const char *interfaz);

// Busca la ruta hacia destinationIp leyendo la tabla byte a byte mediante la MMU.
// Retorna 0 y copia el nombre de la interfaz en outInterfaz si la encontró, -1 si no hay ruta.
int findRoute(uint32_t destinationIp, char *outInterfaz);

#endif // ROUTINGTABLE_H