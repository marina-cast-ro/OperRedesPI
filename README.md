# CI-0123 - Proyecto Integrador Redes-Oper

**Universidad de Costa Rica**
Facultad de Ingeniería - Escuela de Ciencias de la Computación e Informática

II Semestre, 2026

---

## Equipo Koopa Troopas

| Estudiante                    | Carné  |
| ----------------------------- | ------ |
| Daniel Rodríguez Ruiz         | C4J199 |
| Hermes Josué Rojas Sancho     | C16882 |
| José Andrés Serrano Chavarría | C4J967 |
| Marina Castro Peralta         | C31886 |
| Sebastián Sánchez Jiménez     | C4J761 |

## Profesores

- Maeva Murcia Meléndez
- Adrián Lara Petitdemange

---

## Sobre el repositorio

Este repositorio contiene el desarrollo completo del Proyecto Integrador del curso
CI-0123 (Redes y Sistemas Operativos) a lo largo del II Semestre 2026, incluyendo
documentación, diseño e implementación de cada etapa.

## Etapas

| Etapa | Descripción                            | Estado      |
| ----- | -------------------------------------- | ----------- |
| 1     | Propuesta de protocolo de comunicación | En progreso |

## División del trabajo Etapa 1

| Proceso                 | Estudiante asignado                        |
| ----------------------- | ------------------------------------------ |
| server_user.c           | Daniel Rodríguez                           |
| syscall_send_protocol.c | José Serrano, Marina Castro y Hermes Rojas |
| client_user.c           | Sebastián Sánchez                          |

### Tareas de cada estudiante

#### server_user.c

- [ ] Capturar datos del sensor en la Raspberry Pi y generar el archivo `.txt` local.
- [ ] Implementar la interfaz/llamada hacia `syscall_send_protocol` pasando IP, puerto y ruta del `.txt`.
- [ ] Notificar éxito o fallo del envío al usuario.

#### syscall_send_protocol.c

- [ ] Crear socket UDP y lectura del archivo .txt en bloques (fread).
- [ ] Construir tramas inyectando la Header de 2 bytes y enviar con sendto().
- [ ] Configurar el temporizador de retransmisión (SO_RCVTIMEO) para manejar el timeout.
- [ ] Recibir ACK con recvfrom(), validar el bit alternante (1 - N) y enviar trama FIN al terminar.

#### client_user.c

- [ ] Crear socket UDP y enlazar el puerto destino con bind().
- [ ] Bucle de recepción con recvfrom(), extracción de Header y filtrado de tramas duplicadas.
- [ ] Generar y responder el ACK con el bit conmutado (1 - N).
- [ ] Escribir los datos recibidos en el archivo .txt local, procesar trama FIN y notificar al usuario.

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
## Cómo ejecutar

_Pendiente: instrucciones de build y ejecución._
