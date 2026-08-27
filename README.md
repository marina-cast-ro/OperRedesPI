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

| Proceso       	 			| Estudiante asignado  |
| ----------------------------- | ------ |
| server_user.c    				| Daniel Rodríguez |
| syscall_send_protocol.c   	| José Serrano, Marina Castro y Hermes Rojas |
| client_user.c 				| Sebastián Sánchez |

## Diagrama de Secuencia del Protocolo

```mermaid
%%{init: {'theme': 'dark'}}%%
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



## Cómo ejecutar

_Pendiente: instrucciones de build y ejecución._
