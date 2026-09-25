# División del trabajo Etapa 2

| Módulo                                     | Estudiante |
| ------------------------------------------ | ---------- |
| Configuración, Precarga y Build System     | Hermes     |
| Protocolo y Serialización                  | Sebas      |
| Motor de Red con pthread                   | Jose       |
| Conmutación y Propagación (Lógica Central) | Marina     |
| Nodos Host y CLI                           | Daniel     |

# Diagrama de flujo 

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