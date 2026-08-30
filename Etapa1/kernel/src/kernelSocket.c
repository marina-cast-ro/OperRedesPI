#include "../include/kernelSocket.h"

// Esto ya está incluido en el protocol.h pero lo agrego acá para localizar problemas localmente
// en si falta alguna librería o algo que instalar
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/types.h>
#include <linux/inet.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/errno.h>

int ksocketCreate(struct socket **socket_out) {
    int error = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, socket_out);
    if (error < 0) {
        pr_err("ksocketCreate: Error al crear el Kernel socket, error=%d\n", error);
        return error;
    }
    return 0;
}

void ksocketRelease(struct socket *socket) {
    if (socket) sock_release(socket);
}

// Enviar y recibir datos a nivel de Kernel
int ksocket_sendto(struct socket *socket, const char *ip_dest, int port, const void *buffer, size_t length) {
    struct sockaddr_in address;  // Dirección IPv4 para el envío de datos
    struct msghdr message;       // El "sobre" del mensaje
    struct kvec kBuffer;         // Buffer de datos en espacio Kernel

    // Validación de parámetros, retorna EINVAL (Invalid argument del Kernel) si hay algo inválido
    if (!socket || !ip_dest || !buffer || length == 0) 
        return -EINVAL; 

    // Configuración de la dirección del socket
    memset(&address, 0, sizeof(address));
    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = in_aton(ip_dest);

    // Configuración del mensaje
    memset(&message, 0, sizeof(message));
    message.msg_name    = &address;
    message.msg_namelen = sizeof(address);

    // Configuración del contenedor de datos para el Kernel
    kBuffer.iov_base = (void*)buffer;
    kBuffer.iov_len  = length;

    // Verificación del envío correcto de datos
    int error = kernel_sendmsg(socket, &message, &kBuffer, 1, length);
    if (error < 0) pr_err("ksocket_sendto: Fallo al enviar, error=%d\n", error);

    return error;
}

int ksocket_recvfrom(struct socket *socket, void *buffer, size_t length, long timeout_ms) {
    struct msghdr message;       // El "sobre" del mensaje
    struct kvec kBuffer;         // Buffer de datos en espacio Kernel
    
    // Validación de parámetros, retorna EINVAL (Invalid argument del Kernel) si hay algo inválido
    if (!socket || !buffer || length == 0) 
        return -EINVAL;

    // Configuración de timeout
    // TODO: El struct de "socket" tengo que definirlo e inicializarlo fuera de este código
    (timeout_ms > 0)
        ? socket->sk->sk_rcvtime = msecs_to_jiffies(timeout_ms);
        : socket->sk->sk_rcvtimeo = MAX_SCHEDULE_TIMEOUT;

    // Configuración del mensaje y contenedor de datos para el Kernel
    memset(&message, 0, sizeof(message));
    kBuffer.iov_base = buffer;
    kBuffer.iov_len  = length;

    int error = kernel_recvmsg(socket, &msg, &vec, 1, length, msg.msg_flags);
    if (error < 0) {
        (error == -EAGAIN)
            ? pr_warn("ksocket_recvfrom: Timeout esperando datos\n");
            : pr_err("ksocket_recvfrom: Fallo al recibir, error=%d\n", error);
    }

    return error;
}