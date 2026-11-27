# Registro de Depuración y Tiempos — `Kernel + Stop-and-Wait` (Etapa 1)

## Datos generales

- **Fecha:** 27-11-2026
- **Archivo base:** `../prototipo_ia/kernel_ia.c`
- **Archivo corregido:** `kernel_corregido.c`

## Medición de tiempo (Paso 2)

| Actividad                          | Hora inicio | Hora fin | Duración (min) |
| ---------------------------------- | ----------- | -------- | -------------- |
| Revisión y detección de errores    |             |          |                |
| Corrección en `kernel_corregido.c` |             |          |                |
| Verificación / pruebas             |             |          |                |
| **Total Paso 2**                   |             |          | **0 min**      |

## Errores encontrados y corregidos

| #   |    Ubicación / Función    | Error cometido por la IA | Corrección aplicada |
| --- | ------------------------- | ------------------------ | ------------------- |
| 1   | `setup_kernel_udp_socket()` | En el caso de que `kernel_bind()` falle (devuelve un valor negativo), el código de la IA nunca hace `sock_release()`, dejando memoria ocupada pero inútil en el espacio del Kernel que no se puede liberar hasta un reinicio forzoso del sistema.                       | Agregar `sock_release(g_ksock)` y `g_ksock = NULL` en esta sección de la función.                    |
| 2   |                           |                          |                     |
| 3   |                           |                          |                     |

## Comparativa vs. implementación manual (Punto 3.a)

| Métrica                           | Etapa 1 (Manual) | Etapa 5 (Con IA)     | Diferencia |
| --------------------------------- | ---------------- | -------------------- | ---------- |
| Generación / Programación inicial | _Pendiente_      | _Pendiente (Paso 1)_ |            |
| Depuración y corrección           | _Pendiente_      | _Pendiente (Paso 2)_ |            |
| **Tiempo total**                  | **_Pendiente_**  | **_Pendiente_**      |            |

**Conclusión breve:** _Pendiente_
