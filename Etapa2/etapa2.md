# División del trabajo Etapa 2

| Módulo                             | Estudiante asignado |
| ---------------------------------- | ------------------- |
| Sockets y Redes                    | Jose y Sebastian    |
| Arquitectura MMU y Memoria Virtual | Hermes y Marina     |
| Syscall, Header y CLI              | Daniel              |

## Fecha ideal para terminar módulos y comenzar a probar integración

Antes del martes 21 de septiembre (clase de PI).

## Tareas de cada estudiante

### Tareas en Conjunto  

- [ ] Crear el archivo protocol.h donde definimos la estructura del paquete (IP de origen, IP de destino, tipo de mensaje y datos de control).
- [ ] Crear el archivo mmu.h con los nombres de las funciones que vamos a usar para leer, escribir e inicializar la memoria simulada.
- [ ] Preparar el archivo que compila automáticamente todo el proyecto con un solo comando.
- [ ] Probar todo el sistema de extremo a extremo.

### Módulo 1: Sockets y Redes (Dos personas)

- [ ] Hacer que el enrutador lea su archivo de configuración (config.txt) al arrancar para saber a cuáles puertos y vecinos conectarse.
- [ ] Dejar el enrutador listo y escuchando para recibir mensajes entrantes de los hosts o de otros enrutadores.
- [ ] Enviar un mensaje inicial a los enrutadores vecinos (usando la syscallSendFrame) para avisar que ya está encendido y conectado.

---

- [ ] Cada vez que un host o enrutador avise cuál es su IP, mandarle esa información al Módulo 2 para que la guarde en la memoria.
- [ ] Cuando llegue un mensaje para alguien, extraer la IP de destino, preguntarle a la memoria (Módulo 2) por cuál cable/puerto debe salir y reenviarlo usando la syscallSendFrame.

### Módulo 2: Arquitectura MMU y Memoria Virtual (Dos personas)

- [x] Crear las estructuras para traducir direcciones virtuales a físicas (dividiendo en páginas de 8 bytes) y el sistema de reemplazo rápido FIFO.
- [x] Programar la lógica que toma una dirección virtual y la convierte en una dirección física en la RAM.
- [x] Gestionar la actualización de entradas dentro del TLB.

---

- [ ] Declarar un arreglo de bytes (uint8_t RAM[]) que funcionará como la memoria física del enrutador.
- [ ] Crear las funciones mmu_read_byte y mmu_write_byte que leen y escriben en la RAM pasando por la traducción de direcciones.
- [ ] Hacer la función que guarda la tabla de rutas en la RAM (IP $\rightarrow$ Puerto de salida) y la función que busca por cuál puerto mandar un paquete leyendo byte a byte esa RAM.

### Módulo 3: Syscall, Header y CLI (Una persona)

- [ ] Compilar e instalar la función syscallSendFrame en el sistema operativo de los hosts y de la Raspberry Pi para que todos envíen mensajes usando la misma puerta de salida del kernel.
- [ ] Hacer que cuando un usuario encienda su programa cliente, este envíe automáticamente un paquete a su enrutador avisando: "Hola, esta es mi IP".
- [ ] Crear la parte del programa que escucha y recibe los mensajes finales en la computadora de destino para mostrarlos en pantalla.
- [ ] Crear la interfaz de comandos donde el usuario puede elegir si quiere ejecutar el programa en modo --enviar un mensaje o modo --escuchar.
