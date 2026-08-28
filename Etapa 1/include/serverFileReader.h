#ifndef FILEREADER_H
#define FILEREADER_H

#include "protocol.h"


FILE* openInputFile(const char *filePath);
size_t readNextPayload(FILE *file, uint8_t *payloadBuffer);
void closeInputFile(FILE *file);

#endif