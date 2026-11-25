# Etapa 5 — Retrospectiva con IA

En esta etapa se realiza una retrospectiva sobre el uso de grandes modelos de lenguaje (LLMs) en el desarrollo de software de sistemas operativos y redes. Para ello, se seleccionaron dos de los componentes de mayor complejidad técnica implementados manualmente en etapas anteriores del proyecto, se generaron prototipos asistidos por IA siguiendo las especificaciones del curso, se depuraron los errores encontrados y se comparó el esfuerzo, el tiempo invertido y las implicaciones de aprendizaje, seguridad y ejercicio profesional.

## Aspectos Evaluados

| Aspecto | Etapa de origen | Descripción |
| ------- | --------------- | ----------- |
| `fsDisk` | Etapa 4 | Simulación de disco de 32 KB (sectores de 128 B, bloques de 512 B, bitmaps e inodos) para almacenamiento de registros ROA. |
| `Kernel + Stop-and-Wait` | Etapa 1 | Syscall/módulo en espacio de kernel de Linux con socket y envío confiable mediante protocolo Stop-and-Wait. |

Para cada aspecto se conserva tanto el código original generado por el modelo de lenguaje (`prototipo_ia/`) como la versión depurada tras corregir los errores detectados (`corregido/`), junto con el registro de los tiempos de generación y depuración.

## Estructura de carpetas

```text
Etapa5/
├── fsDisk/
│   ├── prototipo_ia/        # Prototipo generado por IA (fsDisk_ia.c)
│   └── corregido/           # Versión depurada y correcciones (fsDisk_corregido.c)
├── kernelStopAndWait/
│   ├── prototipo_ia/        # Prototipo generado por IA (kernel_ia.c)
│   └── corregido/           # Versión depurada y correcciones (kernel_corregido.c)
├── presentacion/            # Presentación final del equipo
└── README.md
```

## Entregables

- **Código fuente asociado:** prototipos generados con IA y sus respectivas versiones corregidas para `fsDisk` y `Kernel + Stop-and-Wait`.
- **Presentación:** análisis comparativo de tiempos y esfuerzo (manual vs. IA), errores encontrados durante la depuración y reflexión sobre aprendizaje, seguridad, automatización y panorama laboral.

## Compilación y Verificación

_Pendiente: agregar los comandos de compilación y pruebas una vez incorporados los archivos `.c` de los prototipos y sus versiones corregidas._
