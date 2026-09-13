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
| 2     | Construcción de las tablas de enrutamiento | En progreso |
| 3     | -                                          | -           |
| 4     | -                                          | -           |
| 5     | -                                          | -           |

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
