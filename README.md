<p align="center">
  <img src="assets/koopa-troopas-logo.png" alt="Logo del equipo Koopa Troopas" width="420">
</p>

<h1 align="center">CI-0123 - Proyecto Integrador Redes-Oper</h1>

<p align="center">
  <strong>Universidad de Costa Rica</strong><br>
  Facultad de Ingeniería - Escuela de Ciencias de la Computación e Informática<br>
  II Semestre, 2026
</p>

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

| Etapa | Descripción                                | Estado      |
| ----- | ------------------------------------------ | ----------- |
| 1     | Del Raspberry Pi a la base de datos        | Completada  |
| 2     | Construcción de las tablas de enrutamiento | Completada  |
| 3     | Diseño de RPKI                             | Completada  |
| 4     | Implementación de RPKI                     | Completada  |
| 5     | Retrospectiva con IA                       | En progreso |

## Instrucciones de Uso

### Etapa 1

#### Compilación

> Ubicarse dentro de la carpeta Etapa1/user/

``` bash
 
# Compilación estándar 
make all

# Compilación del código del emisor
make server

# Compilación del código del receptor
make client

# Limpiar los archivos compilados
make clean

```

#### Ejecución

> Ubicarse dentro de la carpeta Etapa1/sensor/

``` bash

# Ejecución real (con sensor de movimiento)
python3 movementSensor.py <ip_dest> <puerto> 

# Ejecución simulada (sin sensor conectado)
python3 simulateSensor.py <ip_dest> <puerto> --interval <segundos> --motion-every <segundos>

```

### Etapa 2

_Pendiente: instrucciones de compilación y ejecución._

> Ubicarse dentro de la carpeta Etapa2/

### Etapa 4

Requiere `gcc`, `make`, `python3`, `openssl` y las bibliotecas de desarrollo de OpenSSL (`sudo apt install libssl-dev`).

#### 1. Compilación y Pruebas Unitarias por Módulo

```bash
# Certificados (truststore y validación X.509)
make -C Etapa4/certificados

# Validación de formato y firma de ROAs (18 casos de prueba)
make -C Etapa4/validacionROAs test

# Repositorio de ROAs con sistema de archivos simulado (puerto TCP 9000)
make -C Etapa4/repositorio test

# Nodo Host (host_emisor, host_receptor, host_datos)
make -C Etapa4/host

# Enrutador (Vector de Distancias + MMU + RPKI)
make -C Etapa4/vector
```

#### 2. Prueba Integral Local (`127.0.0.1`)

Levanta automáticamente 4 islas (`1`, `2`, `4`, `5`), el repositorio de ROAs y los receptores locales, validando convergencia multi-salto, entrega de paquetes `DATA`, los 4 ataques RPKI, estadísticas de MMU/tráfico y reconvergencia ante caída de enlaces:

```bash
./red/simular_casa.sh --auto-test
```

Para ejecutar la batería de los **4 ataques de seguridad RPKI** de forma independiente:

```bash
# Modo local (127.0.0.1: repo 19000, router 15002)
./red/probar_ataques.sh --local all

# Modo laboratorio (repo 172.16.20.10:9000, router 172.16.20.1:5005)
./red/probar_ataques.sh --lab --repo <IP_REPO>:9000 --router 172.16.20.1:5005 all
```

#### 3. Ejecución en Laboratorio Físico (Isla 2 - Koopatroopas)

El entorno se despliega en 3 máquinas conectadas al switch Cisco 2960 (`red/sw-isla-2.txt`):

1. **Repositorio ROA (`Fa0/9`):**
   ```bash
   # Configura IP (VLAN 20 local, VLAN 99 global o modo dual) e inicia build/repoServer
   sudo ./red/iniciar_repositorio.sh --red <INTERFAZ>
   # O directamente con IP ya configurada:
   cd Etapa4/repositorio && ./build/repoServer ../certificados/trusted-roots.pem 9000
   ```

2. **Host Emisor y Receptor (`Fa0/2`, IP por DHCP):**
   *(Requiere contar localmente con la llave privada `Etapa4/certificados/nodo.key` correspondiente a `nodo.crt`)*
   ```bash
   # Menú interactivo (Opción 4: publica ROA en bucle --loop y levanta host_receptor en :5005)
   ./red/iniciar_emisor_receptor.sh
   ```
   También permite enviar paquetes `DATA` mediante la Opción 2 o directamente:
   ```bash
   cd Etapa4/host
   ./host_datos 172.16.20.1 5005 2 <ISLA_DESTINO> "<MENSAJE>"
   ```

3. **Enrutador Linux (`Fa0/3` Troncal):**
   ```bash
   # Configura subinterfaces VLAN (20, 112, 124, 125, 99) y asegura net.ipv4.ip_forward=0
   sudo ./red/configurar_enrutador.sh <INTERFAZ>

   # Iniciar enrutador (actualizando "servidor_local" en config_koopatroopas.json con la IP DHCP del Host)
   cd Etapa4/vector
   ./enrutador --red config_koopatroopas.json [<IP_REPO>:<PUERTO>]
   ```
   *(Al detener el enrutador con `Ctrl+C`, envía aviso de muerte `-1` e imprime el reporte de la MMU y de tráfico `DATA`).*
