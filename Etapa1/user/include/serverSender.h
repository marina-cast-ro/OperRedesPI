#ifndef SERVERSENDER_H
#define SERVERSENDER_H

#include "protocol.h"
#include "serverFileReader.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int sendFileOverProtocol(const char *filePath, const char *ip, int port);

#endif