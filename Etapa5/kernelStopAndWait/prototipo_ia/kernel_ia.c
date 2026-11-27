/*
 * Prototipo generado con IA — Etapa 5 (Paso 1)
 * Aspecto elegido: Llamada al sistema en Kernel Space + Protocolo Stop-and-Wait sobre UDP (Etapa 1)
 * Equipo: Koopa Troopas (CI-0123 Proyecto Integrador Redes y Sistemas Operativos)
 *
 * Especificación dada en el prompt:
 * - Crear dos syscalls en el kernel de Linux: initProtocol() y sendFrame(ip_dest, port, buffer, len).
 * - Usar sockets UDP en espacio de kernel (sock_create_kern, kernel_sendmsg, kernel_recvmsg).
 * - Encabezado empaquetado de 4 bytes: type (0=DATA, 1=ACK, 2=END), seqNumber (0 o 1), payloadLength.
 * - Payload máximo de 512 bytes.
 * - Protocolo confiable Stop-and-Wait con bit alternante (0 y 1), timeout de espera de ACK (100 ms)
 *   y retransmisión automática hasta un máximo de 15 intentos.
 */

#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>

#define MAX_PAYLOAD_SIZE 512
#define ACK_TIMEOUT_MS   100
#define MAX_RETRIES      15

#define FRAME_DATA 0
#define FRAME_ACK  1
#define FRAME_END  2

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;
    uint8_t  seqNumber;
    uint16_t payloadLength;
} Header;

typedef struct {
    Header  header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} Frame;
#pragma pack(pop)

#define MAX_FRAME_SIZE sizeof(Frame)

static struct socket *g_ksock = NULL;
static uint8_t g_seq_bit = 0;

/* Configura el socket UDP persistente en espacio de kernel */
static int setup_kernel_udp_socket(void)
{
    struct sockaddr_in local_addr;
    struct __kernel_old_timeval tv;
    mm_segment_t oldfs;
    int err;

    if (g_ksock != NULL) {
        sock_release(g_ksock);
        g_ksock = NULL;
    }

    err = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &g_ksock);
    if (err < 0) {
        pr_err("[StopAndWait] Error creando socket en kernel: %d\n", err);
        return err;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(0);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = kernel_bind(g_ksock, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0) {
        pr_err("[StopAndWait] Error en kernel_bind: %d\n", err);
        return err;
    }

    /* Configurar timeout de recepción para esperar el ACK */
    tv.tv_sec  = 0;
    tv.tv_usec = ACK_TIMEOUT_MS * 1000;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    err = kernel_setsockopt(g_ksock, SOL_SOCKET, SO_RCVTIMEO_OLD,
                            (char *)&tv, sizeof(tv));
    set_fs(oldfs);

    return err;
}

/* Syscall 1: Inicializa el estado del protocolo y el bit de secuencia */
SYSCALL_DEFINE0(initProtocol)
{
    g_seq_bit = 0;
    return setup_kernel_udp_socket();
}

/* Envía un paquete UDP desde espacio de kernel */
static int kudp_send(const char *ip_str, int port, void *buf, size_t len)
{
    struct sockaddr_in dest_addr;
    struct msghdr msg;
    struct kvec iov;

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(port);
    dest_addr.sin_addr.s_addr = in_aton(ip_str);

    memset(&msg, 0, sizeof(msg));
    msg.msg_name    = &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);

    iov.iov_base = buf;
    iov.iov_len  = len;

    return kernel_sendmsg(g_ksock, &msg, &iov, 1, len);
}

/* Recibe un paquete UDP en espacio de kernel (bloquea hasta timeout) */
static int kudp_recv(void *buf, size_t len)
{
    struct msghdr msg;
    struct kvec iov;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = buf;
    iov.iov_len  = len;

    return kernel_recvmsg(g_ksock, &msg, &iov, 1, len, 0);
}

/* Syscall 2: Envía una trama confiable usando Stop-and-Wait sobre UDP */
SYSCALL_DEFINE4(sendFrame,
                const char __user *, ip_dest,
                int, port,
                const void __user *, user_buf,
                size_t, length)
{
    uint8_t kbuf[MAX_FRAME_SIZE];
    Header ack_hdr;
    Header *hdr;
    int attempt, ret;

    if (!g_ksock)
        return -ENOTSOCK;

    if (length == 0 || length > MAX_FRAME_SIZE)
        return -EINVAL;

    if (copy_from_user(kbuf, user_buf, length))
        return -EFAULT;

    /* Colocar el número de secuencia actual en el encabezado */
    hdr = (Header *)kbuf;
    hdr->seqNumber = g_seq_bit;

    for (attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ret = kudp_send(ip_dest, port, kbuf, length);
        if (ret < 0) {
            pr_err("[StopAndWait] Error enviando trama en intento %d: %d\n", attempt, ret);
            return ret;
        }

        ret = kudp_recv(&ack_hdr, sizeof(Header));
        if (ret == -EAGAIN || ret == -EWOULDBLOCK) {
            pr_warn("[StopAndWait] Timeout esperando ACK (intento %d/%d), retransmitiendo...\n",
                    attempt, MAX_RETRIES);
            continue;
        }
        if (ret < 0)
            return ret;

        /* Verificar que sea un ACK válido con el número de secuencia correspondiente */
        if (ack_hdr.type == FRAME_ACK && ack_hdr.seqNumber == g_seq_bit) {
            g_seq_bit = 1 - g_seq_bit;
            pr_info("[StopAndWait] Trama confirmada en intento %d\n", attempt);
            return 0;
        }
    }

    pr_err("[StopAndWait] Fallo tras %d intentos sin recibir ACK\n", MAX_RETRIES);
    return -ETIMEDOUT;
}
