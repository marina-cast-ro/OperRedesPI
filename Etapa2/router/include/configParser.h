#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdint.h>
#include "routingTable.h"
#include "router.h" // Incluye la estructura ConfigRouter

// Lee config.txt y precarga los hosts en la MMU llamando a saveRoute(ip, interfaz_virtual).
// La interfaz virtual es el ID estático del archivo que luego será traducido a sockfd por forwarding/sockets.
ConfigRouter parseConfigAndPreload(const char *filename);

// Retorna la IP propia del router que se leyó de config.txt
uint32_t getLocalIp(void);

#endif