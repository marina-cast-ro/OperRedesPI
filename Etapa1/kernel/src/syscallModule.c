#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/protocol.h>
#include <linux/stopAndWait.h>

MODULE_LICENSE("GPL");

// Definición de Syscall para el Kernel Base
SYSCALL_DEFINE4(sendFrame, 
                const char __user *, ip_dest, 
                int, port, 
                const void __user *, user_buffer, 
                size_t, length) 
{
    char kernel_ip[16];
    u8 kernel_buffer[MAX_BUFFER_SIZE];

    // Validar límite de tamaño
    if (length > MAX_BUFFER_SIZE || length == 0) {
        pr_err("[syscallSendFrame] Error: Invalid size (%zu bytes)\n", length);
        return -EINVAL;
    }

    // 2. Copiar dirección IP desde User Space
    if (strncpy_from_user(kernel_ip, ip_dest, sizeof(kernel_ip)) < 0) {
        pr_err("[syscallSendFrame] Error: Copying IP from user space failed\n");
        return -EFAULT;
    }
    kernel_ip[sizeof(kernel_ip) - 1] = '\0';

    // 3. Copiar datos de la trama desde User Space
    if (copy_from_user(kernel_buffer, user_buffer, length)) {
        pr_err("[syscallSendFrame] Error: Copying frame data from user space failed\n");
        return -EFAULT;
    }

    // 4. IMPRIMIR DATOS RECIBIDOS EN DMESG
    pr_info("[syscallSendFrame] --- TRAMA RECIBIDA EN KERNEL ---\n");
    pr_info("[syscallSendFrame] IP Dest : %s\n", kernel_ip);
    pr_info("[syscallSendFrame] Port     : %d\n", port);
    pr_info("[syscallSendFrame] Size     : %zu bytes\n", length);
    pr_info("[syscallSendFrame] Header Type: %u\n", kernel_buffer[0]);

    // 5. Enviar mediante Stop-and-Wait (Futura implementación)
    // int response = sendFrameStopAndWait(kernel_ip, port, (Frame *)kernel_buffer, length);
    // if (response < 0) return response;

    return 0;
}

static int __init syscallModuleInit(void) {
    pr_info("[syscallModule] Syscall module up\n");
    return 0;
}

static void __exit syscallModuleExit(void) {
    pr_info("[syscallModule] Module down\n");
}

module_init(syscallModuleInit);
module_exit(syscallModuleExit);