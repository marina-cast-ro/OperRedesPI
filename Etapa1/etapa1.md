# División del trabajo Etapa 1

| Proceso                 | Estudiante asignado                        |
| ----------------------- | ------------------------------------------ |
| server_user.c           | Daniel Rodríguez                           |
| syscall_send_protocol.c | José Serrano, Marina Castro y Hermes Rojas |
| client_user.c           | Sebastián Sánchez                          |

## Tareas de cada estudiante

### server_user.c

- [x] Capturar datos del sensor en la Raspberry Pi y generar el archivo `.txt` local.
- [x] Implementar la interfaz/llamada hacia `syscall_send_protocol` pasando IP, puerto y ruta del `.txt`.
- [x] Notificar éxito o fallo del envío al usuario.

### syscall_send_protocol.c

- [x] Crear socket UDP y lectura del archivo `.txt` en bloques (`fread`).
- [x] Construir tramas inyectando la Header de 4 bytes y enviar con `sendto()`.
- [x] Configurar el temporizador de retransmisión (`SO_RCVTIMEO`) para manejar el timeout.
- [x] Recibir ACK con `recvfrom()`, validar el bit alternante (1 - N) y enviar trama FIN al terminar.

### client_user.c

- [x] Crear socket UDP y enlazar el puerto destino con `bind()`.
- [x] Bucle de recepción con `recvfrom()`, extracción de Header y filtrado de tramas duplicadas.
- [x] Generar y responder el ACK con el bit conmutado (1 - N).
- [x] Escribir los datos recibidos en el archivo `.txt` local, procesar trama FIN y notificar al usuario.

### Tareas en Conjunto  

- [x] Crear `include/protocol.h` con la `struct Header` de N bytes y armar el `Makefile` del proyecto.

## Diagrama de Secuencia del Protocolo

```mermaid
sequenceDiagram
autonumber
participant S as Servidor - Emisor
participant C as Cliente - Receptor

Note over S,C: Bit Alternante:<br/>Si la trama lleva seq=N (0 o 1), <br/>el ACK responde con seq = 1 - N

Note over S,C: 1. Transferencia de Datos
loop Para cada bloque del archivo de muestras
 S->>C: Enviar Trama (DATOS, seq=N)

 alt Caso A: Transmisión Exitosa
  C-->>S: Responder ACK (ACK, seq = 1 - N)
 else Caso B: Pérdida de Trama o ACK (Timeout)
  Note over S: ⏱️ Timeout sin recibir ACK (seq = 1 - N)
        S->>C: REENVIAR Trama (DATOS, seq=N)
        opt Si el cliente recibe trama repetida (seq=N)
            Note over C: Descarta payload duplicado.
        end
        C-->>S: REENVIAR ACK (ACK, seq = 1 - N)
 end
end

Note over S,C: 2. Finalización 
S->>C: Enviar Trama FIN
C-->>S: Responder ACK FIN
Note over S,C: Cierre de sockets y archivo .txt guardado.
```

## Diagrama de Arquitectura

```mermaid
flowchart TD
    Args["Args / Terminal<br/>• Dirección IP<br/>• Puerto Destino"]

    subgraph U1["Modo Usuario"]
        Server["server_user.c<br/>• Recopila datos del sensor<br/>• Crea .txt con todos los datos<br/>• Llama a la syscall personalizada (envío, UDP confiable)<br/>• Notifica éxito/fallo al usuario"]
    end

    subgraph K["Modo Kernel"]
        Syscall["syscall_send_protocol.c<br/>• Recibe parámetros desde server_user.c (IP, puerto, datos .txt)<br/>• Usa sockets UDP para un protocolo confiable de envío<br/>• Retorna éxito/fallo del envío (boolean)"]
    end

    subgraph U2["Modo Usuario"]
        Client["client_user.c<br/>• Recibe datos vía sockets UDP (confiable)<br/>• Recopila todo en un .txt<br/>• Notifica éxito/fallo al usuario"]
    end

    Note["Implementar primero en modo usuario,<br/>luego adaptar a modo kernel"]:::nota

    Args --> Server
    Server -->|"IP, puerto, datos del sensor (.txt)"| Syscall
    Syscall -.->|"boolean (éxito/fallo)"| Server
    Syscall ==>|"envío UDP confiable"| Client
    Note -.- Syscall

    classDef nota fill:#fff3cd,stroke:#e0a800,color:#664d03;

```
