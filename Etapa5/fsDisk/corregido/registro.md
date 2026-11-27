# Registro de Corrección del Prototipo con IA — `fsDisk` (Etapa 4)

## Datos generales

- **Modelo utilizado en la revisión:** Claude (1 conversación)
- **Archivo revisado:** `prototipo_ia/fsDisk_ia.c` (generado por Gemini, 1 prompt, 23 min — ver registro de Parte 1)
- **Archivo corregido:** `corregido/fsDisk_corregido.c`
- **Cómo se buscaron los errores:** compilación con `-Wall -Wextra -fsanitize=address,undefined`, pruebas propias (fuzz de 20 000 operaciones put/replace/delete con verificador de consistencia de bitmaps, contadores, directorio e inodos) y lectura del código.

## Medición de tiempo (Paso 2)

| Actividad                                       | Hora inicio | Hora fin | Duración (min) |
| ----------------------------------------------- | ----------- | -------- | -------------- |
| Revisión del código e identificación de errores | 8:17 pm     | 8:21 pm  | 4 min          |
| Corrección y pruebas                            | 8:21 pm     | 8:23 pm  | 2 min          |
| **Total Paso 2**                                |             |          | **6 min**      |

## Lo que la IA hizo bien

- Geometría CHS/LBA correcta (biyección verificada en los 256 sectores).
- No confundió sector (128 B) con bloque (512 B).
- Bitmaps y contadores del superbloque coherentes en 20 000 operaciones aleatorias (put / replace / delete con ROAs de 0 a 512 B).
- Compila sin warnings y sus 8 pruebas propias pasan.

## Errores encontrados y corregidos

### Error 1 — No valida que la clave sea una IP/prefijo
- **Dónde:** `is_valid_ip_key()`.
- **Qué pasaba:** aceptaba cualquier texto imprimible sin espacios: `hola`, `999.999.999.999/99`, `10.0.0.0/33`, `///` se guardaban como si fueran IPs.
- **Corrección:** se valida formato IPv4 `a.b.c.d` o `a.b.c.d/n`, con cada octeto en 0..255 y el prefijo `n` en 0..32.
- **Prueba nueva (9):** 4 claves válidas se aceptan y 9 inválidas se rechazan. Falla en el original, pasa en el corregido.

### Error 2 — `delete` no es seguro ante un inodo corrupto (deja el FS a medias)
- **Dónde:** `fs_roa_delete()`.
- **Qué pasaba:** liberaba bloque por bloque sin validar antes. Si encontraba un puntero inválido a la mitad, salía con error pero ya había limpiado bits del bitmap y borrado el contenido de bloques; el superbloque no se actualizaba y el directorio seguía apuntando al inodo. Resultado: ROA "existente" con sus bloques marcados libres y en cero (pérdida de datos y riesgo de que otro ROA reutilice esos bloques). Además, a diferencia de `get`/`put`, no comprobaba `inode.in_use`.
- **Corrección:** antes de liberar nada se comprueba `in_use` y se valida todo el inodo con `fs_inode_count_phys_blocks()` (que ya existía); si algo está mal se devuelve `FS_ERR_CORRUPT` sin modificar el disco.
- **Prueba nueva (10):** inodo con un puntero corrupto; el delete debe fallar y bitmaps/superbloque deben quedar intactos. Falla en el original, pasa en el corregido.

## Resultado

- Original + pruebas nuevas: 8 aprobadas, **2 fallidas** (pruebas 9 y 10).
- Corregido: 10 aprobadas, 0 fallidas; compila sin warnings; sin errores de ASan/UBSan; fuzz de 20 000 operaciones consistente.
- Nota: la validación asume IPv4. Si el proyecto necesita IPv6 hay que ampliarla (además, la entrada de directorio de 32 B limita la clave a 23 caracteres).

