# Registro de Depuración y Tiempos — `fsDisk` (Etapa 4)

## Datos generales

- **Fecha:** 26-11-2026
- **Modelo utilizado en la revisión:** Claude (1 conversación)
- **Archivo base:** `../prototipo_ia/fsDisk_ia.c` (generado por Gemini, 1 prompt, 23 min — ver registro de Parte 1)
- **Archivo corregido:** `fsDisk_corregido.c`
- **Cómo se buscaron los errores:** compilación con `-Wall -Wextra -fsanitize=address,undefined`, pruebas propias (fuzz de 20 000 operaciones put/replace/delete con verificador de consistencia de bitmaps, contadores, directorio e inodos) y lectura del código.

## Medición de tiempo (Paso 2)

| Actividad                          | Hora inicio | Hora fin | Duración (min) |
| ---------------------------------- | ----------- | -------- | -------------- |
| Revisión y detección de errores    | 8:17 pm     | 8:21 pm  | 4 min          |
| Corrección en `fsDisk_corregido.c` | 8:21 pm     | 8:22 pm  | 1 min          |
| Compilación y pruebas básicas      | 8:22 pm     | 8:23 pm  | 1 min          |
| **Total Paso 2**                   |             |          | **6 min**      |

## Lo que la IA hizo bien

- Geometría CHS/LBA correcta (biyección verificada en los 256 sectores).
- No confundió sector (128 B) con bloque (512 B).
- Bitmaps y contadores del superbloque coherentes en 20 000 operaciones aleatorias (put / replace / delete con ROAs de 0 a 512 B).
- Compila sin warnings y sus 8 pruebas propias pasan.

## Errores encontrados y corregidos

| #   | Ubicación / Función | Error cometido por la IA | Corrección aplicada |
| --- | ------------------- | ------------------------ | ------------------- |
| 1   | `is_valid_ip_key()` | No valida que la clave sea una IP: aceptaba `hola`, `999.999.999.999/99`, `10.0.0.0/33` o `///` como clave. | Se valida formato IPv4 `a.b.c.d` o `a.b.c.d/n`, con octetos de 0 a 255 y prefijo de 0 a 32. Prueba nueva (9). |
| 2   | `fs_roa_delete()`   | Con un inodo corrupto liberaba bloques uno por uno y fallaba a la mitad, dejando bitmaps, superbloque y directorio inconsistentes. Además no comprobaba `inode.in_use`, a diferencia de `get`/`put`. | Se comprueba `in_use` y se valida todo el inodo con `fs_inode_count_phys_blocks()` antes de liberar nada. Prueba nueva (10). |
| 3   | —                   | (solo se encontraron 2 errores) | — |

**Resultado de las pruebas:** original + pruebas nuevas: 8 aprobadas, 2 fallidas (9 y 10). Corregido: 10 aprobadas, 0 fallidas, sin errores de ASan/UBSan, fuzz de 20 000 operaciones consistente.

**Nota:** la validación asume IPv4. Si el proyecto necesita IPv6 hay que ampliarla (además, la entrada de directorio de 32 B limita la clave a 23 caracteres).

## Comparativa vs. implementación manual (Punto 3.a)

| Métrica                           | Etapa 4 (Manual) | Etapa 5 (Con IA)                  | Diferencia         |
| --------------------------------- | ---------------- | --------------------------------- | ------------------ |
| Generación / Programación inicial | ~4.5 a 5 h       | 23 min (prompt 16 + generación 7) | ~4 h menos         |
| Depuración y corrección           | ~2 a 2.5 h       | 6 min                             | ~2 h menos         |
| **Tiempo total**                  | **~6.5 a 7.5 h** | **29 min**                        | **~6 a 7 h menos** |

**Conclusión breve:** La corrección fue rápida porque la IA revisó el código con pruebas dirigidas (claves inválidas, inodos corruptos, consistencia de bitmaps). Aun así, alguien debe entender esas pruebas para saber si el código corregido es correcto.