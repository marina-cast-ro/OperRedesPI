# División del trabajo Etapa 2

| Módulo                             | Estudiante asignado |
| ---------------------------------- | ------------------- |
| Sockets y Redes                    |                     |
| Arquitectura MMU y Memoria Virtual |                     |
| Syscall, Header y CLI              |                     |

## Fecha ideal para terminar módulos y comenzar a probar integración

Antes del viernes 18 de septiembre (clase de PI).

## Tareas de cada estudiante

### Tareas en Conjunto  

- [ ] Crear include/protocol.h con la estructura del Header de $N$ bytes (IPs + control de la Etapa 1) y armar el Makefile del proyecto.
- [ ] Crear include/mmu.h con las firmas públicas de la memoria (mmu_read_byte, mmu_write_byte, mmu_init).

### Módulo 1: Sockets y Redes (Dos personas)

- [ ] Leer el archivo de configuración local (config.txt) al arrancar el nodo enrutador.
- [ ] Levantar el socket de escucha en el puerto correspondiente para recibir conexiones entrantes.
- [ ] Enviar el mensaje de anuncio inicial hacia las direcciones de los enrutadores vecinos.

---

- [ ] Recibir anuncios de red e invocar al Módulo 2 para registrar las rutas en la memoria simulada.
- [ ] Implementar la función de reenvío (forwarding): extraer la IP destino del encabezado y consultar la interfaz de salida a la MMU.
- [ ] Reenviar el paquete por la interfaz física correspondiente respetando el formato de la trama.

### Módulo 2: Arquitectura MMU y Memoria Virtual (Dos personas)

- [ ] Diseñar la estructura de la Tabla de Páginas y del TLB según los parámetros de paginación.
- [ ] Implementar la función de traducción de dirección virtual a dirección física ($\text{DirVirt} \rightarrow \text{DirFis}$).
- [ ] Gestionar la actualización de entradas dentro del TLB.

---

- [ ] Declarar el arreglo físico de bytes (uint8_t RAM[]) para la memoria simulada.
- [ ] Implementar mmu_read_byte(dir_virt) y mmu_write_byte(dir_virt, valor) pasando por la traducción de direcciones.
- [ ] Implementar la función para serializar y escribir entradas de enrutamiento byte a byte en la RAM.
- [ ] Implementar la función de búsqueda de rutas que recorre la memoria mediante mmu_read_byte para retornar la interfaz de salida.

### Módulo 3: Syscall, Header y CLI (Una persona)

- [ ] Modificar la syscall de envío en espacio de kernel para incorporar la estructura del encabezado con los campos de enrutamiento (IP Destino y Origen) junto con el control de flujo de la Etapa 1.
- [ ] Implementar la recepción de datos en espacio de usuario (usando sockets estándar) para extraer el encabezado y procesar los mensajes recibidos.
- [ ] Crear el CLI unificado del nodo final manejando los parámetros por consola para los modos de ejecución (--enviar / --escuchar).
- [ ] Validar y ejecutar las pruebas del flujo completo de punta a punta: Nodo Emisor (Syscall Kernel) -> Socket Enrutador Pi (MMU) -> Nodo Receptor (Usuario)
