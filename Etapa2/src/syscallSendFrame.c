#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "frameRouting.h"
#include "../../Etapa1/user/include/protocol.h"

long syscallSendFrame(const char *ip, int port, const void *frame, size_t length) {
    return syscall(SYS_UDP_RELIABLE, ip, port, frame, length);
}
