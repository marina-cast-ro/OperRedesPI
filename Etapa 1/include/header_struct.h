#include <stdint.h>

typedef enum {
	FRAME_DATA = 0,	// Trama de datos
	FRAME_ACK = 1,	// Trama de acuse de recibo (Acknowledgment)
	FRAME_END = 2 	// Trama de finalización de comunicación
} FrameType;

// __attribute__((packed)) hace que la estructura no tenga relleno y ocupe exactamente el tamaño de sus campos
typedef struct __attribute__((packed)) Header {	
	FrameType type;
	uint8_t seq_number;
} Header;

