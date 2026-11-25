# Registro de Prototipo con IA — `fsDisk` (Etapa 4)

## Datos generales

- **Modelo utilizado:** _Gemini 4 Neon (Razonamiento: Extra High)_
- **Fecha:** _24-11-2026_
- **Archivo generado:** `fsDisk_ia.c`
- **Cantidad de prompts / iteraciones:** _1_

## Medición de tiempo (Paso 1)

| Actividad                           | Hora inicio | Hora fin | Duración (min) |
| ----------------------------------- | ----------- | -------- | -------------- |
| Redacción del prompt                | 10:08 pm    | 10:24 pm | 16 min         |
| Generación e iteraciones con el LLM | 10:28 pm    | 10:35 pm | 7 min          |
| **Total Paso 1**                    |             |          | **23 min**     |

## Prompt utilizado

```text
Crea un archivo en C (fsDisk_ia.c) usando únicamente la biblioteca estándar de C que simule un disco en memoria de 32 KB con la siguiente geometría CHS: 2 platos, 16 pistas por plato, 8 sectores por pista, 128 bytes por sector (256 sectores LBA). La capa lógica debe ser con bloques de 512 bytes (4 sectores por bloque, 64 bloques totales):

Bloque 0: Superbloque (con número mágico para verificar inicialización, contador de versión, bloques totales, bloques libres, inodos totales e inodos libres).
Bloque 1: Bitmap de inodos.
Bloque 2: Bitmap de bloques de datos.
Bloques 3-10: Tabla de 32 inodos de 128 bytes cada uno (con 4 punteros directos + 1 puntero indirecto simple, dueño, tamaño y timestamps).
Bloque 11: Directorio raíz con entradas de 32 bytes que mapeen IP a inodeIndex.
Bloques 12–63: Bloques de datos.

Operaciones básicas que debe implementar: traducir CHS a LBA, leer/escribir sector y bloque, formatear el disco. Además, las operaciones de alto nivel para guardar/reemplazar (put), consultar (get), borrar (delete) y listar (list) ROAs (hasta 512 B por ROA) manteniendo el contador de versión.

Debes implementar además un Makefile (compilando con gcc -Wall -Wextra) que permita compilar y ejecutar el programa de forma fácil, y un menú interactivo sencillo en terminal que permita probar cada operación y ver el estado del disco simulado.
```

## Verificación inicial (sin modificar el código)

- **Líneas generadas:** _2038 líneas en fsDisk_ia.c (+ 20 líneas en Makefile)_
- **Compilación inicial (`gcc -Wall -Wextra -c fsDisk_ia.c`):** _Compila a la primera sin errores ni warnings._
- **Observaciones iniciales:** _El modelo generó en una sola respuesta el archivo completo con el código solicitado junto con el Makefile. Además, creó el menú interactivo en terminal solicitado, verificaciones Static_assert para el tamaño de las estructuras y una suite de 8 pruebas automáticas propia (./fsDisk_ia --test), las cuales pasan (8/8). Queda pendiente la identificación y corrección de errores en el Paso 2._
