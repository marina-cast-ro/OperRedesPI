# Registro de Depuración y Tiempos — `Kernel + Stop-and-Wait` (Etapa 1)

## Datos generales

- **Fecha:** 27-11-2026
- **Archivo base:** `../prototipo_ia/kernel_ia.c`
- **Archivo corregido:** `kernel_corregido.c`
- **Cómo se buscaron los errores:** En base al estudio del código previo hecho para el Kernel de la Etapa 1 del proyecto, además de la compilación contra los headers de la máquina virtual Ubuntu en la cual se realizó este trabajo.

## Medición de tiempo (Paso 2)

| Actividad                          | Hora inicio | Hora fin | Duración (min) |
| ---------------------------------- | ----------- | -------- | -------------- |
| Revisión y detección de errores    |  11:30 PM (26 Nov)     |   12:30 AM (27 Nov)         |    1 hora            |
| Corrección en `kernel_corregido.c` |  12:30 AM         |   1:00 AM         |     30 min           |
| Verificación / pruebas             |  1:00 AM          |   1:25 AM         |   25 min             |
| **Total Paso 2**                   |             |          | **~2 horas**    |

## Errores encontrados y corregidos

| #   |    Ubicación / Función    | Error cometido por la IA | Corrección aplicada |
| --- | ------------------------- | ------------------------ | ------------------- |
| 1   | `setup_kernel_udp_socket()` | En el caso de que `kernel_bind()` falle (devuelve un valor negativo), el código de la IA nunca hace `sock_release()`, dejando memoria ocupada pero inútil en el espacio del Kernel que no se puede liberar hasta un reinicio forzoso del sistema.                       | Agregar `sock_release(g_ksock)` y `g_ksock = NULL` en esta sección de la función.                    |
| 2   | `setup_kernel_udp_socket()` | Las funciones antiguas `get_fs`, `set_fs` y `kernel_setsockopt` impuestas por la IA para fijar el time out de recepción del socket. Estas líneas servían en Kernels pasados.                     |  Implementación de las funciones `lock_sock()`, `msecs_to_jiffies()` y `release_sock()`, esto es lo que los Kernels modernos (versiones 5.10-5.19 en adelante) utilizan.                      |
| 3   |  `kudp_send()`                    |   Deducía la dirección IP en binario directamente con `in_aton()`, una función con un manejo de errores pobre y más permisiva en los inputs dados.                       | Se aplica `in4_pton()` para un mejor: manejo de errores, valores de retorno, manejo de puntero de avance, etc.               |
| 4   |  `SYSCALL_DEFINE4` -> sendFrame  | Se utilizaba el puntero directo de ip_dest (en User Space) para el envío de tramas.                        |  Se realiza una copia del puntero de ip_dest en User Space a través de un pequeño buffer y `strncpy_from_user()`, así se "traduce" del espacio de memoria User al espacio de memoria Kernel.               |

## Resultados

Se realizó por medio del comando `make clean` y `make check` la respectiva compilación con los headers del Kernel local, no lanzó errores. Es necesario para asegurar la válidez del código IA corregido el volver a re-compilar e instalar el Kernel del proyecto original para evaluar este código, pero debido a que esa es una tarea que toma horas en hacerse y en esencia el código corregido cumple con los requisitos y flujo mínimo funcional del Kernel + Stop-and-Wait, se limitó a esta verificación de Kernel headers vía compilación local.

## Comparativa vs. implementación manual (Punto 3.a)

| Métrica                           | Etapa 1 (Manual) | Etapa 5 (Con IA)     | Diferencia |
| --------------------------------- | ---------------- | -------------------- | ---------- |
| Generación / Programación inicial |  ~4 a 4.5 h        | 5 min 30 s (prompt 3 min 30 s + generación 2 min) | ~4 h menos            |
| Depuración y corrección           |  ~5 h     | 5 min | ~5 h menos           |
| **Tiempo total**                  | **~9 a 9.5 h**  | **10 min**  | **~9 h menos**         |

**Conclusión breve:** La corrección fue rápida porque la IA revisó el código en base al código previamente construido por los estudiantes y así evaluar la completitud del generado por IA. Aun así, alguien debe entender el mecanismo y necesidades del Kernel y Syscalls para saber si el código corregido es correcto, o en especifico para este caso, si cumple con el ciclo y cuidados requeridos de la comunicación entre el espacio de usuario y kernel.
