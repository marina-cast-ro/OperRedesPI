#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include "protocol.h"
#include "stopAndWait.h"

MODULE_LICENSE("GPL");

SYSCALL_DEFINE4(send_frame, 
                const char __user *, ip_dest, 
                int, port, 
                const void __user *, user_buffer, 
                size_t, length) 
{
    char kernel_ip[16];
    u8 kernel_buffer[MAX_BUFFER_SIZE];
    int response;

    // 1. Validar límite de tamaño
    if (length > MAX_BUFFER_SIZE || length == 0) {
        return -EINVAL;
    }

    // 2. Copiar dirección IP (usando strncpy_from_user para strings terminadas en nulo)
    if (strncpy_from_user(kernel_ip, ip_dest, sizeof(kernel_ip)) < 0) {
        return -EFAULT;
    }
    kernel_ip[sizeof(kernel_ip) - 1] = '\0';

    // 3. Copiar datos de la trama desde User Space
    if (copy_from_user(kernel_buffer, user_buffer, length)) {
        return -EFAULT;
    }

    // 4. Enviar mediante el módulo Stop-and-Wait
    // response = sendFrameStopAndWait(kernel_ip, port, (Frame *)kernel_buffer, length);
    // if (response < 0) {
    //     return response;
    // }

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