#ifndef SERVERSENDER_H
#define SERVERSENDER_H

#include "protocol.h"
#include "serverFileReader.h"

int sendFileOverProtocol(const char *filePath, const char *ip, int port);

#endif