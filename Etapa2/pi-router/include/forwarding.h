#ifndef FORWARDING_H
#define FORWARDING_H

// Procesa cualquier trama entrante recibida desde un socket:
// - ANNOUNCE: Registra la IP del router vecino directamente conectado y mapea su sockfd real.
// - ADVERTISE: Registra/actualiza en la MMU rutas hacia hosts/redes remotas a través del sockfd del vecino.
// - DATA: Consulta findRoute(ip_destino, &out_sockfd) y conmuta el paquete por el socket de salida.
void processPacket(const char *buffer, int sockfd);

// Transmite la trama ANNOUNCE propia (router.localIp) a los demás routers vecinos 
// para establecer la adyacencia de red al arrancar el servicio. Asocia el sockfd saliente en la MMU.
void sendInitialAnnounce(void);

// Lee de la MMU la IP de la PC local precargada por configParser y transmite 
// tramas ADVERTISE a los demás routers para anunciar su alcanzabilidad.
void sendInitialAdvertise(void);

#endif