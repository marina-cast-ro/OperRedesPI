# Registro de Prototipo con IA — `Kernel + Stop-and-Wait` (Etapa 1)

## Datos generales

- **Modelo utilizado:** _Gemini 4_
- **Fecha:** _26-11-2026_
- **Archivo generado:** `kernel_ia.c`
- **Cantidad de prompts / iteraciones:** _1_

## Medición de tiempo (Paso 1)

| Actividad                                      | Hora inicio | Hora fin | Duración       |
| ---------------------------------------------- | ----------- | -------- | -------------- |
| Redacción del prompt con la especificación     | 04:18 pm    | 04:21 pm | 3 min 30 s     |
| Generación con el LLM (`25 s`) y revisión inicial (`1 min 35 s`) | 04:21 pm | 04:23 pm | 2 min 00 s |
| **Total Paso 1**                               |             |          | **5 min 30 s** |

## Prompt utilizado

```text
Actúa como desarrollador del kernel de Linux en C. Necesito un prototipo en un solo archivo .c para la Etapa 1 de un proyecto de Redes y Sistemas Operativos (en una Raspberry Pi con Linux) que cumpla esta especificación:
1. Definir dos llamadas al sistema (SYSCALL_DEFINE): initProtocol() para inicializar el estado y abrir un socket UDP persistente en espacio de kernel, y sendFrame(const char __user *ip_dest, int port, const void __user *user_buf, size_t length) para enviar una trama desde espacio de usuario hacia un receptor remoto.
2. El envío debe hacerse desde espacio de kernel usando sockets UDP del kernel (sock_create_kern, kernel_bind, kernel_sendmsg, kernel_recvmsg).
3. Definir un encabezado empaquetado de 4 bytes (Header: uint8_t type con 0=DATA, 1=ACK, 2=END; uint8_t seqNumber que alterna entre 0 y 1; uint16_t payloadLength) seguido de un payload máximo de 512 bytes.
4. Implementar el protocolo confiable Stop-and-Wait sobre UDP: inyectar el bit de secuencia actual en el encabezado, enviar el datagrama, esperar un ACK con un temporizador de 100 ms y retransmitir hasta un máximo de 15 intentos si hay timeout (-EAGAIN), alternando el bit de secuencia al confirmar la recepción.
```

## Verificación inicial (sin modificar el código)

- **Líneas generadas:** _184 líneas en `kernel_ia.c` (+ 13 líneas en `Makefile`)_
- **Estado inicial / compilación o revisión estática:** _A nivel estructural define las dos syscalls solicitadas, las estructuras empaquetadas (`Header` y `Frame`) y el ciclo de retransmisiones sobre UDP. Sin embargo, falla al compilar contra encabezados de kernels modernos (Linux 5.10+ / 6.x) por el uso de APIs eliminadas (`get_fs`, `set_fs`, `kernel_setsockopt`)._
- **Observaciones iniciales:** _Además de las APIs obsoletas del kernel, en una revisión estática se observan errores de frontera Usuario/Kernel (`ip_dest` pasado sin `strncpy_from_user`), falta de liberación de socket ante fallo de `kernel_bind` y discrepancia en el bit esperado del `ACK`, los cuales quedan documentados para su depuración en el Paso 2 (`../corregido/`)._
