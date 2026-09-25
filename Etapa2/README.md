# División del trabajo Etapa 2

| Módulo                                     | Estudiante |
| ------------------------------------------ | ---------- |
| Configuración, Precarga y Build System     | Hermes     |
| Protocolo y Serialización                  | Sebas      |
| Motor de Red con pthread                   | Jose       |
| Conmutación y Propagación (Lógica Central) | Marina     |
| Nodos Host y CLI                           | Daniel     |

## Tareas de cada estudiante

### Tareas en Conjunto  

- [x] Probar todo el sistema de extremo a extremo.
- [x] **Depuración de concurrencia:** Correr las pruebas usando AddressSanitizer y ThreadSanitizer para asegurar que no existan fugas de memoria ni *race conditions* en la MMU.
- [x] La MMU usa un uint32_t. No se van a usar strings como "eth0". Se va a guardar y buscar directamente el File Descriptor del socket abierto (como si fuera la interfaz).

### Persona 1: Configuración, Precarga y Build System

**Archivos:** configParser.c, Makefile y pruebas.

- [x] Configuración: Programar la lectura de config.txt para extraer la IP local y los vecinos.
- [x] Precarga: Hacer los llamados iniciales a saveRoute() para cargar las conexiones directas en la memoria virtual antes de levantar la red.
- [x] Compilación: Crear el Makefile centralizado definiendo las reglas para compilar todos los .c con la bandera -pthread.
- [x] Testing: Hacer un main de pruebas para probar la MMU de 256 bytes y asegurar que no haya desbordamientos.

---

### Persona 2: Protocolo y Serialización

**Archivos:** protocol.c, protocol.h

- [x] Decodificación: Programar decodeFrame(), asegurando limpiar caracteres residuales o saltos de línea (\n, \r, \0) del buffer del socket para no rechazar tramas válidas.
- [x] Codificación: Añadir la firma en el .h e implementar encodeFrame() para transformar el struct en el string exacto TIPO|IP|DATOS\n.
- [x] Conversión: Utilizar solamnte inet_pton e inet_ntop para manejar las IPs en binario de red (evitar htonl/ntohl).

---

### Persona 3: Motor de Red con pthread

**Archivos:** router.c, router.h

- [x] Arranque: Programar initRouterListen() configurando el socket (bind, listen) y lanzando el hilo con pthread_create.
- [x] Bucle Pasivo: Programar la rutina del hilo listening() usando recv(). No toma decisiones lógicas: su única función es recibir bytes y pasar el buffer crudo y el sockfd a la función de enrutamiento (De persona 4).
- [x] Apagado: Programar endRouterListen() para cerrar los descriptores de archivo y matar el hilo limpiamente.

### Persona 4: Conmutación y Propagación (Lógica Central)

**Archivos:** forwarding.c, forwarding.h

- [x] Protección de Memoria: Declarar e inicializar un pthread_mutex_t. Es obligatorio bloquear y liberar el mutex justo antes y después de cada llamada a saveRoute o findRoute para no corromper la MMU.
- [x] Anuncios (Generación): Programar la emisión de un mensaje ANNOUNCE propio por los sockets activos apenas el router encienda.
- [x] Propagación: En la función principal processPacket(buffer, sockfd): si recibe ANNOUNCE o ADVERTISE, guardarlo con saveRoute(). Si la ruta resulta ser nueva, generar un ADVERTISE y reenviarlo a los demás routers vecinos.
- [x] Conmutación: Si el paquete recibido es DATA, buscar el destino con findRoute() y reenviarlo hacia ese FD usando send().

### Persona 5: Nodos Host y CLI

**Archivos:** listener.c, sender.c, main.c, listener.h, sender.h

- [x] Receptor: Programar keepListening() configurando el socket cliente pasivo, y showMessage() para imprimir datos en terminal.
- [x] Emisor: Programar sendMessage() para que el cliente empaquete y mande datos al router.
- [x] CLI (Interfaz de comandos): Programar main.c con el manejo de argumentos (argc/argv) para que el usuario decida si el ejecutable arranca en modo router, emisor o receptor.

# Diagrama de flujo de archivos y datos

```mermaid
flowchart TD

subgraph group_capture["Captura de mediciones"]
  node_sensor["Sensor de movimiento<br/>[movementSensor.py]"]
  node_simulator["Simulador de sensor<br/>[simulateSensor.py]"]
  node_storage[("Almacenamiento auxiliar<br/>[storage.py]")]
end

subgraph group_stage1["Transporte Etapa 1"]
  node_kernel_syscalls["Syscalls de protocolo<br/>[syscallModule.c]"]
  node_stop_wait["Stop-and-wait<br/>[stopAndWait.c]"]
  node_kernel_socket["Socket de kernel<br/>[kernelSocket.c]"]
  node_user_server["Servidor emisor<br/>[serverMain.c]"]
  node_file_reader["Lector de archivos<br/>[serverFileReader.c]"]
  node_frame_builder["Constructor de tramas"]
  node_sender["Emisor de datos<br/>[serverSender.c]"]
  node_client["Receptor UDP<br/>[clientUser.c]"]
  node_sensor_output[("Archivo de mediciones")]
end

subgraph group_stage2["Red Etapa 2"]
  node_host_send["Host emisor<br/>[sender.c]"]
  node_host_listen["Host listener<br/>[listener.c]"]
  node_app_main["Modos de ejecución<br/>[main.c]"]
  node_router["Router TCP<br/>[router.c]"]
end

subgraph group_routing["Estado y rutas"]
  node_config["Parser de configuración<br/>[configParser.c]"]
  node_forwarding["Procesador de paquetes<br/>[forwarding.c]"]
  node_routing_table[("Tabla de rutas<br/>[routingTable.c]")]
  node_mmu["MMU<br/>[mmu.c]"]
  node_virtual_memory[("Memoria virtual<br/>[virtualMemory.c]")]
end

node_operator(("Operador"))
node_network_peer(("Router vecino"))
node_host_user(("Usuario de red"))

node_operator -->|"lee movimiento"| node_sensor
node_operator -->|"simula mediciones"| node_simulator
node_kernel_syscalls -->|"envía trama"| node_stop_wait
node_stop_wait -->|"usa socket"| node_kernel_socket
node_client -->|"escribe mediciones"| node_sensor_output
node_app_main -->|"inicia escucha"| node_host_listen
node_app_main -->|"inicia envío"| node_host_send
node_app_main -->|"lee configuración"| node_config
node_app_main -->|"inicializa"| node_mmu
node_app_main -->|"inicializa"| node_virtual_memory
node_app_main -->|"inicia router"| node_router
node_host_user -->|"elige modo"| node_app_main
node_host_listen -->|"conecta y escucha"| node_router
node_host_send -->|"envía mensaje"| node_router
node_router -->|"acepta conexión"| node_network_peer
node_router -->|"registra vecino"| node_routing_table
node_router -->|"procesa trama"| node_forwarding
node_routing_table -->|"lee y escribe bytes"| node_mmu
node_mmu -->|"accede memoria"| node_virtual_memory

click node_sensor "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/sensor/movementSensor.py"
click node_simulator "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/sensor/simulateSensor.py"
click node_storage "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/sensor/storage.py"
click node_kernel_syscalls "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/kernel/src/syscallModule.c"
click node_stop_wait "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/kernel/src/stopAndWait.c"
click node_kernel_socket "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/kernel/src/kernelSocket.c"
click node_user_server "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/user/src/serverMain.c"
click node_file_reader "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/user/src/serverFileReader.c"
click node_frame_builder "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/user/src/serverFrameBuilder.c"
click node_sender "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/user/src/serverSender.c"
click node_client "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa1/user/src/clientUser.c"
click node_host_send "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/host/src/sender.c"
click node_host_listen "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/host/src/listener.c"
click node_app_main "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/main.c"
click node_router "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/router/src/router.c"
click node_config "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/router/src/configParser.c"
click node_forwarding "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/router/src/forwarding.c"
click node_routing_table "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/router/src/routingTable.c"
click node_mmu "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/router/src/mmu.c"
click node_virtual_memory "https://github.com/marina-cast-ro/operredespi/blob/main/Etapa2/router/src/virtualMemory.c"

classDef toneNeutral fill:#f8fafc,stroke:#334155,stroke-width:1.5px,color:#0f172a
classDef toneBlue fill:#dbeafe,stroke:#2563eb,stroke-width:1.5px,color:#172554
classDef toneAmber fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#78350f
classDef toneMint fill:#dcfce7,stroke:#16a34a,stroke-width:1.5px,color:#14532d
classDef toneRose fill:#ffe4e6,stroke:#e11d48,stroke-width:1.5px,color:#881337
classDef toneIndigo fill:#e0e7ff,stroke:#4f46e5,stroke-width:1.5px,color:#312e81
classDef toneTeal fill:#ccfbf1,stroke:#0f766e,stroke-width:1.5px,color:#134e4a
class node_sensor,node_simulator,node_storage toneBlue
class node_kernel_syscalls,node_stop_wait,node_kernel_socket,node_user_server,node_file_reader,node_frame_builder,node_sender,node_client,node_sensor_output toneAmber
class node_host_send,node_host_listen,node_app_main,node_router,node_network_peer toneMint
class node_config,node_forwarding,node_routing_table,node_mmu,node_virtual_memory toneRose
class node_operator,node_host_user toneIndigo
```

# Comandos para la ejecución del programa

> Ubicarse dentro de la carpeta Etapa2/

### Compilación

``` bash

# Compilación para todo el proyecto
make

```

### Ejecución

Es recomendado que para esta sección se abran múltiples ventanas de línea de comandos (terminal) o estén activas distintas computadoras para observar el envío y recepción de datos entre distintos dispositivos para así asegurar el flujo intencionado en esta etapa.

#### Conocimiento de la dirección IP del dispositivo actual

``` bash

# Obtiene la IP local
hostname -I

```

*La obtención de la IP local es un paso crucial para la ejecución de este sistema de routers, receptores y emisores. Es necesaria para que el router funcione correctamente para el posterior envío (--send) y recibo (--listen) de datos.*

#### Router

``` bash

# Ejecución para el arranque de router
./build/routing --router config.txt

```

*El archivo (Argumento 3) dado es el que contiene la información base de puerto, IP, hosts locales directos y routers vecinos del router propio.*

#### Computadora receptora

``` bash

# Ejecución para el arranque del dispositivo receptor
./build/routing --listen <IP de la computadora> 8080 <IP Host>

```

***1. Argumento 3 (IP de la computadora):*** *Es la dirección IP de la computadora (obtenida con ***hostname -I***) en donde el router se encuentra ejecutándose.*  

***2. Argumento 5 (IP Host):*** *Debe ser la misma que uno de los hosts locales directos (especificados en el archivo config.txt) del router.*

#### Computadora emisora

``` bash

# 1. Envío de datos local (envía a un host directo del router)
# Ejecución para el arranque del dispositivo emisor
./build/routing --send <IP real del router> 8080 <IP Host> "Prueba local"

# 2. Envío de datos externo (envía a un host externo al router)
# Ejecución para el arranque del dispositivo emisor
./build/routing --send <IP real del router> 8080 <IP Externa> "Hola Grupo Y"

```

*1. ***El argumento 3 (IP propia)*** coresponde a la dirección IP del dispositivo actual. En caso de que el router se ubique en la misma máquina donde se hace el --send se utiliza la IP **127.0.0.1**, en caso contrario sería la IP real (la obtenida con hostname -I).*  

*2. ***El argumento 5 (IP Host / Externa)*** corresponde al dispositivo destinatario.*

### Limpieza y borrado de archivos

``` bash

# Limpieza y borrado para todo el proyecto
make clean

```